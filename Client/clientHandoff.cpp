#include "client.h"
#include <errno.h>
#include <netinet/tcp.h>

/*
    posibles criterios para el cambio de protocolo:
    - Si la transferencia de archivos UDP está tardando demasiado
    - Si se detecta pérdida de paquetes en UDP??
    - Si el tamaño del archivo supera un umbral específico
    - Congestión de red
    - Variación de latencia
    - Calidad de conexión
    - Microfragmentación en UDP segun el MTU

*/

bool should_switch_protocol(TransferState* state, int socket) {
    if (state->current_protocol == PROTOCOL_TCP && state->bytes_sent >= state->total_size / 2) {
        struct tcp_info tcp_stats;
        socklen_t tcp_info_len = sizeof(tcp_stats);
        
        if (getsockopt(socket, IPPROTO_TCP, TCP_INFO, &tcp_stats, &tcp_info_len) == 0) {
            printf("unas metricas de TCP:\n");
            printf("  - retransmisiones totales: %u\n", tcp_stats.tcpi_total_retrans);
            printf("  - paquetes retransmitidos: %u\n", tcp_stats.tcpi_retrans);
            printf("  - Estado de conexión: %u\n", tcp_stats.tcpi_state);
            
            if (tcp_stats.tcpi_total_retrans > 0) {
                printf("%u retransmisiones\n", tcp_stats.tcpi_total_retrans);
                return false;
            }
            
            printf("cambio TCP -> UDP\n");
            return true;
        } else {
            printf("error  TCP_INFO\n");
            return false;
        }
    }
    
    return false;
}

int setup_udp_connection() {
    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        printf("Error al crear socket UDP\n");
        return -1;
    }
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        printf("Error al conectar UDP\n");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int setup_tcp_connection() {
    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Error S-TCP\n");
        return -1;
    }
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    
    int retries = 5;
    while (retries > 0) {
        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
            printf("Conexión TCP establecida\n");
            return sockfd;
        }
        
        printf("Reintentando conexión TCP");
        usleep(200000);
        retries--;
    }

    printf("Error C-TCP\n");
    close(sockfd);
    return -1;
}

int send_control_message(int socket, int msg_type, size_t resume_pos) {
    printf("mensaje de control\n");
    
    MessageHeader header;
    header.magic = htonl(MAGIC_CONTROL);
    header.type = htonl(MSG_CONTROL);
    header.length = htonl(sizeof(ControlMessage));
    
    ControlMessage msg;
    msg.type = htonl(msg_type);
    msg.resume_position = htonl(resume_pos);
    
    if (send(socket, &header, sizeof(header), 0) != sizeof(header)) {
        printf("error enviando header de control\n");
        return -1;
    }
    
    if (send(socket, &msg, sizeof(msg), 0) != sizeof(msg)) {
        printf("error enviando mensaje de control\n");
        return -1;
    }
    
    printf("control enviado exitosamente\n");
    return 0;
}

int receive_control_message(int socket, ControlMessage* msg) {
    struct timeval timeout;
    timeout.tv_sec = 5; 
    timeout.tv_usec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    MessageHeader header;
    ssize_t result = recv(socket, &header, sizeof(header), 0);

    if (result != sizeof(header)) {
        printf("error recibiendo header de control\n");
        return -1;
    }
    
    header.magic = ntohl(header.magic);

    if (header.magic != MAGIC_CONTROL) {
        printf("magic number inválido \n");
        return -1;
    }
    
    result = recv(socket, msg, sizeof(*msg), 0);
    if (result != sizeof(*msg)) {
        printf("error recibiendo mensaje de control\n");
        return -1;
    }
    
    msg->type = ntohl(msg->type);
    msg->resume_position = ntohl(msg->resume_position);

    printf("todo bien %d\n", msg->type);
    return 0;
}

int switch_protocol(TransferState* state, int* socket) {
    ControlMessage response;
    
    if (state->current_protocol == PROTOCOL_TCP) {
        // TCP -> UDP
        printf("iniciando cambio TCP -> UDP\n");
        
        if (send_control_message(*socket, MSG_SWITCH_TO_UDP, state->bytes_sent) < 0) {
            printf("error enviando mensaje de cambio a UDP\n");
            return -1;
        }
        printf("Esperando confirmación del servidor...\n");

        if (receive_control_message(*socket, &response) < 0) {
            printf("error recibiendo confirmación del servidor\n");
            return -1;
        }
        
        if (response.type != MSG_PROTOCOL_READY) {
            printf("respuesta del server error: %d\n", response.type);
            return -1;
        }
        
        printf("cerrando socket TCP\n");
        close(*socket);
        usleep(300000);
        printf("estableciendo nueva conexión UDP...\n");
        usleep(1000000);

        *socket = setup_udp_connection();
        if (*socket < 0) {
            printf("error estableciendo conexión UDP\n");
            return -1;
        }
        printf("Socket UDP creado exitosamente\n");
        
        char sync_byte = 0x01;
        printf("Enviando byte de sincronización UDP...\n");
        if (send(*socket, &sync_byte, 1, 0) != 1) {
            printf("Error enviando byte de sincronización");
            close(*socket);
            return -1;
        }
        printf("Byte de sincronización UDP enviado exitosamente\n");
        
        usleep(100000);
        
        state->current_protocol = PROTOCOL_UDP;
        printf("cambio a UDP completado\n");
        
    } else {
        // UDP -> TCP
        printf("Iniciando cambio UDP -> TCP\n");
        
        if (send_control_message(*socket, MSG_SWITCH_TO_TCP, state->bytes_sent) < 0) {
            printf("Error enviando mensaje de cambio a TCP\n");
            return -1;
        }
        
        if (receive_control_message(*socket, &response) < 0) {
            printf("Error recibiendo confirmación del servidor\n");
            return -1;
        }
        
        if (response.type != MSG_PROTOCOL_READY) {
            printf("Respuesta inesperada del servidor: %d\n", response.type);
            return -1;
        }
        
        close(*socket);
        usleep(300000); // 300ms
        
        *socket = setup_tcp_connection();
        if (*socket < 0) return -1;
        
        state->current_protocol = PROTOCOL_TCP;
        printf("Cambio a TCP completado exitosamente\n");
    }
    
    return 0;
}

int client_with_handoff() {
    std::string filename = "archivoDePrueba.txt";

    TransferState state;
    int socket;
    char buffer[BUFFER_SIZE];
    
    state.filename = filename;
    state.current_protocol = PROTOCOL_TCP;
    state.bytes_sent = 0;
    state.packet_count = 0;
    state.total_size = 0;
    
    // Abrir archivo
    std::ifstream archivo(filename, std::ios::binary);
    if (!archivo) {
        printf("Error abriendo archivo\n");
        return -1;
    }
    
    // Obtener tamaño
    archivo.seekg(0, std::ios::end);
    state.total_size = archivo.tellg();
    archivo.seekg(0, std::ios::beg);
    
    printf("Archivo: %s (%zu bytes)\n", filename.c_str(), state.total_size);
    
    // Conexión inicial TCP
    socket = setup_tcp_connection();
    if (socket < 0) return -1;
    
    size_t filename_length = htonl(filename.length());
    if (!sendAll(socket, &filename_length, sizeof(filename_length))) {
        printf("error enviando longitud del nombre\n");
        close(socket);
        return -1;
    }
    
    if (!sendAll(socket, filename.data(), filename.length())) {
        printf("error enviando nombre\n");
        close(socket);
        return -1;
    }
    
    size_t file_size = htonl(state.total_size);
    if (!sendAll(socket, &file_size, sizeof(file_size))) {
        printf("error enviando tamaño\n");
        close(socket);
        return -1;
    }

    printf("Info del archivo enviada\n");
    
    while (state.bytes_sent < state.total_size) {

        if (should_switch_protocol(&state, socket)) {
            if (switch_protocol(&state, &socket) < 0) {
                printf("sera de dios\n");
                close(socket);
                return -1;
            }
        }
        
        size_t bytes_to_send = std::min((size_t)BUFFER_SIZE, state.total_size - state.bytes_sent);
        archivo.read(buffer, bytes_to_send);
        
        ssize_t sent = send(socket, buffer, bytes_to_send, 0);
        if (sent != (ssize_t)bytes_to_send) {
            printf("error enviando datos\n");
            close(socket);
            return -1;
        }
        
        state.bytes_sent += bytes_to_send;
        state.packet_count++;
        
        printf("Progreso: %zu/%zu bytes (%.1f%%) - Protocolo: %s\n", 
               state.bytes_sent, state.total_size, 
               (double)state.bytes_sent / state.total_size * 100,
               state.current_protocol == PROTOCOL_TCP ? "TCP" : "UDP");
        
        if (state.current_protocol == PROTOCOL_UDP) {
            //usleep(100);
        }
    }
    
    printf("Termino bien\n");
    archivo.close();
    close(socket);
    return 0;
}
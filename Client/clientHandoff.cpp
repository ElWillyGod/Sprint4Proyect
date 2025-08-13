#include "client.h"

// Función vacía que determina cuándo cambiar de protocolo
bool should_switch_protocol(TransferState* state) {
    // Implementar criterio aquí - por ahora siempre false
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
        printf("Error al crear socket TCP\n");
        return -1;
    }
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        printf("Error al conectar TCP\n");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int send_control_message(int socket, int msg_type, size_t resume_pos) {
    MessageHeader header;
    header.magic = htonl(MAGIC_CONTROL);
    header.type = htonl(MSG_CONTROL);
    header.length = htonl(sizeof(ControlMessage));
    
    ControlMessage msg;
    msg.type = htonl(msg_type);
    msg.resume_position = htonl(resume_pos);
    
    // Enviar header primero
    if (send(socket, &header, sizeof(header), 0) != sizeof(header)) {
        printf("Error enviando header de control\n");
        return -1;
    }
    
    // Enviar mensaje de control
    if (send(socket, &msg, sizeof(msg), 0) != sizeof(msg)) {
        printf("Error enviando mensaje de control\n");
        return -1;
    }
    return 0;
}

int receive_control_message(int socket, ControlMessage* msg) {
    // El header ya fue leído previamente
    if (recv(socket, msg, sizeof(*msg), 0) != sizeof(*msg)) {
        printf("Error recibiendo mensaje de control\n");
        return -1;
    }
    
    msg->type = ntohl(msg->type);
    msg->resume_position = ntohl(msg->resume_position);
    return 0;
}

int switch_protocol(TransferState* state, int* socket) {
    ControlMessage response;
    
    if (state->current_protocol == PROTOCOL_UDP) {
        // UDP -> TCP
        if (send_control_message(*socket, MSG_SWITCH_TO_TCP, state->bytes_sent) < 0) {
            return -1;
        }
        
        if (receive_control_message(*socket, &response) < 0 || response.type != MSG_PROTOCOL_READY) {
            return -1;
        }
        
        close(*socket);
        *socket = setup_tcp_connection();
        if (*socket < 0) return -1;
        
        state->current_protocol = PROTOCOL_TCP;
        
    } else {
        // TCP -> UDP
        if (send_control_message(*socket, MSG_SWITCH_TO_UDP, state->bytes_sent) < 0) {
            return -1;
        }
        
        if (receive_control_message(*socket, &response) < 0 || response.type != MSG_PROTOCOL_READY) {
            return -1;
        }
        
        close(*socket);
        *socket = setup_udp_connection();
        if (*socket < 0) return -1;
        
        state->current_protocol = PROTOCOL_UDP;
    }
    
    return 0;
}

int client_with_handoff() {
    std::string filename = "archivoDePrueba.txt";
    TransferState state;
    int socket;
    char buffer[BUFFER_SIZE];
    
    // Inicializar estado
    state.filename = filename;
    state.current_protocol = PROTOCOL_UDP;
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
    
    // Conexión inicial UDP
    socket = setup_udp_connection();
    if (socket < 0) return -1;
    
    // Enviar metadatos iniciales
    size_t filename_length = htonl(filename.length());
    if (!sendAll(socket, &filename_length, sizeof(filename_length))) {
        printf("Error enviando longitud del nombre\n");
        close(socket);
        return -1;
    }
    
    if (!sendAll(socket, filename.data(), filename.length())) {
        printf("Error enviando nombre del archivo\n");
        close(socket);
        return -1;
    }
    
    size_t file_size = htonl(state.total_size);
    if (!sendAll(socket, &file_size, sizeof(file_size))) {
        printf("Error enviando tamaño del archivo\n");
        close(socket);
        return -1;
    }
    
    // Transferir archivo con handoff
    while (state.bytes_sent < state.total_size) {
        // Verificar si necesita cambio de protocolo
        if (should_switch_protocol(&state)) {
            if (switch_protocol(&state, &socket) < 0) {
                printf("Error en handoff\n");
                close(socket);
                return -1;
            }
        }
        
        // Enviar chunk con header
        size_t bytes_to_send = std::min((size_t)BUFFER_SIZE, state.total_size - state.bytes_sent);
        archivo.read(buffer, bytes_to_send);
        
        // Preparar header para datos
        MessageHeader header;
        header.magic = htonl(MAGIC_DATA);
        header.type = htonl(MSG_DATA);
        header.length = htonl(bytes_to_send);
        
        // Enviar header primero
        if (send(socket, &header, sizeof(header), 0) != sizeof(header)) {
            printf("Error enviando header de datos\n");
            close(socket);
            return -1;
        }
        
        // Enviar datos
        if (send(socket, buffer, bytes_to_send, 0) != (ssize_t)bytes_to_send) {
            printf("Error enviando datos\n");
            close(socket);
            return -1;
        }
        
        state.bytes_sent += bytes_to_send;
        state.packet_count++;
    }
    
    archivo.close();
    close(socket);
    return 0;
}

#include "server.h"
#include <errno.h>

// Variables globales para UDP client address
struct sockaddr_in client_addr;
socklen_t client_addr_len = sizeof(client_addr);
bool client_addr_set = false;

int setup_udp_server() {
    int server_fd;
    struct sockaddr_in address;
    
    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_fd < 0) {
        printf("Error creando socket UDP\n");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Error en bind UDP\n");
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

int setup_tcp_server() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Error creando socket TCP\n");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Error en bind TCP\n");
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 1) < 0) {
        printf("Error en listen\n");
        close(server_fd);
        return -1;
    }
    
    printf("Servidor TCP esperando conexión...\n");
    client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_fd < 0) {
        printf("Error en accept\n");
        close(server_fd);
        return -1;
    }
    
    printf("Cliente TCP conectado exitosamente\n");
    close(server_fd);
    return client_fd;
}

int send_control_message(int socket, int msg_type, size_t resume_pos) {
    printf("Servidor enviando mensaje de control tipo %d\n", msg_type);
    
    MessageHeader header;
    header.magic = htonl(MAGIC_CONTROL);
    header.type = htonl(MSG_CONTROL);
    header.length = htonl(sizeof(ControlMessage));
    
    ControlMessage msg;
    msg.type = htonl(msg_type);
    msg.resume_position = htonl(resume_pos);
    
    // Ahora ambos UDP y TCP pueden usar send() porque UDP está "conectado"
    if (send(socket, &header, sizeof(header), 0) != sizeof(header)) {
        printf("Error enviando header de control\n");
        return -1;
    }
    
    if (send(socket, &msg, sizeof(msg), 0) != sizeof(msg)) {
        printf("Error enviando mensaje de control\n");
        return -1;
    }
    
    printf("Servidor: mensaje de control enviado exitosamente\n");
    return 0;
}

int receive_control_message(int socket, ControlMessage* msg) {
    // El header ya fue leído en el bucle principal
    ssize_t result = recv(socket, msg, sizeof(*msg), 0);
    if (result != sizeof(*msg)) {
        printf("Error recibiendo mensaje de control (recibido: %zd, esperado: %zu)\n", 
               result, sizeof(*msg));
        return -1;
    }
    
    msg->type = ntohl(msg->type);
    msg->resume_position = ntohl(msg->resume_position);
    return 0;
}

bool has_control_message_available(int socket) {
    // Verificar si hay datos disponibles para leer sin bloquear
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(socket, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000; // 1ms timeout
    
    if (select(socket + 1, &readfds, NULL, NULL, &timeout) > 0) {
        // Hay datos disponibles, verificar si es control message
        MessageHeader header;
        ssize_t result = recv(socket, &header, sizeof(header), MSG_PEEK); // MSG_PEEK no consume datos
        
        if (result == sizeof(header)) {
            header.magic = ntohl(header.magic);
            printf("Detectado magic: 0x%X (%s)\n", header.magic, 
                   header.magic == MAGIC_CONTROL ? "CONTROL" : "DATA/OTHER");
            return (header.magic == MAGIC_CONTROL);
        }
    }
    return false;
}

int server_with_handoff() {
    TransferState state;
    state.bytes_received = 0;
    state.current_protocol = PROTOCOL_TCP;
    state.packet_count = 0;
    state.total_size = 0;
    
    int socket;
    char buffer[BUFFER_SIZE];
    std::ofstream archivo;
    
    // Inicializar con TCP
    socket = setup_tcp_server();
    if (socket < 0) return -1;
    
    printf("Servidor TCP iniciado en puerto %d\n", PORT);
    
    // Recibir metadatos iniciales
    size_t filename_length;
    
    // Para TCP inicialmente, usar recv normal
    int socket_type;
    socklen_t optlen = sizeof(socket_type);
    getsockopt(socket, SOL_SOCKET, SO_TYPE, &socket_type, &optlen);
    
    if (socket_type == SOCK_STREAM) {
        // TCP: usar recv normal
        if (!recvAll(socket, &filename_length, sizeof(filename_length))) {
            printf("Error recibiendo longitud del nombre TCP\n");
            close(socket);
            return -1;
        }
    } else {
        // UDP: capturar dirección del cliente
        if (recvfrom(socket, &filename_length, sizeof(filename_length), 0, 
                     (struct sockaddr*)&client_addr, &client_addr_len) != sizeof(filename_length)) {
            printf("Error recibiendo longitud del nombre UDP\n");
            close(socket);
            return -1;
        }
        client_addr_set = true;
        printf("Dirección del cliente UDP capturada\n");
        
        // Conectar el socket del servidor al cliente para poder usar send/recv
        if (connect(socket, (struct sockaddr*)&client_addr, client_addr_len) < 0) {
            printf("Error conectando socket UDP del servidor al cliente\n");
            close(socket);
            return -1;
        }
        printf("Socket UDP del servidor conectado al cliente\n");
    }
    filename_length = ntohl(filename_length);
    
    // Crear buffer temporal para el nombre del archivo
    char* temp_filename = new char[filename_length + 1];
    if (!recvAll(socket, temp_filename, filename_length)) {
        printf("Error recibiendo nombre del archivo\n");
        delete[] temp_filename;
        close(socket);
        return -1;
    }
    temp_filename[filename_length] = '\0';
    state.filename = std::string(temp_filename);
    delete[] temp_filename;
    
    size_t file_size;
    if (!recvAll(socket, &file_size, sizeof(file_size))) {
        printf("Error recibiendo tamaño del archivo\n");
        close(socket);
        return -1;
    }
    state.total_size = ntohl(file_size);
    
    printf("Recibiendo archivo: %s (%zu bytes)\n", state.filename.c_str(), state.total_size);
    
    // Abrir archivo para escritura
    archivo.open(state.filename, std::ios::binary);
    if (!archivo) {
        printf("Error creando archivo\n");
        close(socket);
        return -1;
    }
    
    // Recibir archivo con handoff optimizado
    while (state.bytes_received < state.total_size) {
        // Verificar si hay mensaje de control disponible
        if (has_control_message_available(socket)) {
            printf("Procesando mensaje de control\n");
            // Procesar mensaje de control
            MessageHeader header;
            if (recv(socket, &header, sizeof(header), 0) != sizeof(header)) {
                printf("Error recibiendo header de control\n");
                break;
            }
            
            ControlMessage msg;
            if (receive_control_message(socket, &msg) < 0) {
                break;
            }
            
            printf("Mensaje de control recibido: tipo %d\n", msg.type);
            
            if (msg.type == MSG_SWITCH_TO_UDP) {
                printf("Procesando cambio TCP -> UDP\n");
                
                // Enviar confirmación antes de cerrar el socket
                if (send_control_message(socket, MSG_PROTOCOL_READY, 0) < 0) {
                    printf("Error enviando MSG_PROTOCOL_READY\n");
                    break;
                }
                printf("MSG_PROTOCOL_READY enviado exitosamente\n");
                
                // Dar tiempo para que el cliente reciba la respuesta
                usleep(500000); // 500ms - tiempo suficiente para que el cliente reciba
                
                archivo.close();
                close(socket);
                printf("Socket TCP cerrado\n");
                
                // Configurar servidor UDP
                socket = setup_udp_server();
                if (socket < 0) {
                    printf("Error configurando servidor UDP\n");
                    break;
                }
                printf("Servidor UDP configurado exitosamente en puerto %d\n", PORT);
                
                // Configurar timeout para recvfrom
                struct timeval timeout;
                timeout.tv_sec = 10;  // 10 segundos timeout
                timeout.tv_usec = 0;
                setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                
                // Esperar primera conexión UDP del cliente para capturar su dirección
                printf("Esperando reconexión del cliente UDP (timeout 10s)...\n");
                char sync_buffer[1];
                client_addr_len = sizeof(client_addr);
                ssize_t recv_result = recvfrom(socket, sync_buffer, 1, 0, 
                                             (struct sockaddr*)&client_addr, &client_addr_len);
                if (recv_result != 1) {
                    printf("Error esperando reconexión UDP del cliente (resultado: %zd, errno: %d)\n", 
                           recv_result, errno);
                    close(socket);
                    break;
                }
                client_addr_set = true;
                printf("Cliente UDP reconectado exitosamente\n");
                printf("Dirección capturada: %s:%d (byte sync: 0x%02X)\n", 
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), sync_buffer[0]);
                
                // Conectar el socket del servidor al cliente
                if (connect(socket, (struct sockaddr*)&client_addr, client_addr_len) < 0) {
                    printf("Error conectando socket UDP del servidor al cliente (errno: %d)\n", errno);
                    close(socket);
                    break;
                }
                printf("Socket UDP del servidor conectado al cliente exitosamente\n");
                
                // Reabrir archivo en posición correcta
                archivo.open(state.filename, std::ios::binary | std::ios::in | std::ios::out);
                if (!archivo) {
                    printf("Error reabriendo archivo\n");
                    break;
                }
                archivo.seekp(state.bytes_received);
                printf("Archivo reabierto en posición %zu\n", state.bytes_received);
                
                state.current_protocol = PROTOCOL_UDP;
                printf("Cambio a UDP completado exitosamente\n");
                continue;
                
            } else if (msg.type == MSG_SWITCH_TO_TCP) {
                printf("Procesando cambio UDP -> TCP\n");
                
                // Enviar confirmación
                if (send_control_message(socket, MSG_PROTOCOL_READY, 0) < 0) {
                    break;
                }
                
                // Dar tiempo para que el cliente reciba la respuesta
                usleep(500000); // 500ms
                
                archivo.close();
                close(socket);
                
                socket = setup_tcp_server();
                if (socket < 0) break;
                
                // Reabrir archivo en posición correcta
                archivo.open(state.filename, std::ios::binary | std::ios::in | std::ios::out);
                archivo.seekp(state.bytes_received);
                
                state.current_protocol = PROTOCOL_TCP;
                printf("Cambio a TCP completado exitosamente\n");
                continue;
            }
            
        } else {
            // Recibir datos directamente (SIN header)
            size_t bytes_to_receive = std::min((size_t)BUFFER_SIZE, state.total_size - state.bytes_received);
            ssize_t result = recv(socket, buffer, bytes_to_receive, 0);
            
            if (result <= 0) {
                if (result == 0) {
                    printf("Conexión cerrada por el cliente\n");
                } else {
                    printf("Error recibiendo datos: %d\n", errno);
                }
                break;
            }
            
            archivo.write(buffer, result);
            archivo.flush();
            state.bytes_received += result;
            state.packet_count++;
            
            printf("Progreso: %zu/%zu bytes (%.1f%%) - Protocolo: %s\n", 
                   state.bytes_received, state.total_size, 
                   (double)state.bytes_received / state.total_size * 100,
                   state.current_protocol == PROTOCOL_TCP ? "TCP" : "UDP");
        }
    }
    
    if (state.bytes_received == state.total_size) {
        printf("Transferencia completada exitosamente\n");
    } else {
        printf("Transferencia incompleta: %zu/%zu bytes\n", state.bytes_received, state.total_size);
    }
    
    archivo.close();
    close(socket);
    return 0;
}
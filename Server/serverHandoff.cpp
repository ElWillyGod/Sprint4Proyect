#include "server.h"

int setup_udp_server() {
    int server_fd;
    struct sockaddr_in address;
    
    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_fd < 0) {
        printf("Error creando socket UDP\n");
        return -1;
    }
    
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
    
    client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_fd < 0) {
        printf("Error en accept\n");
        close(server_fd);
        return -1;
    }
    
    close(server_fd);
    return client_fd;
}

int send_control_message(int socket, int msg_type, size_t resume_pos) {
    ControlMessage msg;
    msg.type = htonl(msg_type);
    msg.resume_position = htonl(resume_pos);
    
    if (send(socket, &msg, sizeof(msg), 0) != sizeof(msg)) {
        printf("Error enviando mensaje de control\n");
        return -1;
    }
    return 0;
}

int receive_control_message(int socket, ControlMessage* msg) {
    if (recv(socket, msg, sizeof(*msg), 0) != sizeof(*msg)) {
        printf("Error recibiendo mensaje de control\n");
        return -1;
    }
    
    msg->type = ntohl(msg->type);
    msg->resume_position = ntohl(msg->resume_position);
    return 0;
}

bool is_control_message(int socket) {
    // Verificar si hay datos disponibles para leer
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(socket, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    return select(socket + 1, &readfds, NULL, NULL, &timeout) > 0;
}

int server_with_handoff() {
    TransferState state = {0};
    int socket;
    char buffer[BUFFER_SIZE];
    std::ofstream archivo;
    
    // Inicializar con UDP
    state.current_protocol = PROTOCOL_UDP;
    socket = setup_udp_server();
    if (socket < 0) return -1;
    
    // Recibir metadatos iniciales
    size_t filename_length;
    if (!recvAll(socket, &filename_length, sizeof(filename_length))) {
        printf("Error recibiendo longitud del nombre\n");
        close(socket);
        return -1;
    }
    filename_length = ntohl(filename_length);
    
    state.filename.resize(filename_length);
    if (!recvAll(socket, &state.filename[0], filename_length)) {
        printf("Error recibiendo nombre del archivo\n");
        close(socket);
        return -1;
    }
    
    size_t file_size;
    if (!recvAll(socket, &file_size, sizeof(file_size))) {
        printf("Error recibiendo tamaño del archivo\n");
        close(socket);
        return -1;
    }
    state.total_size = ntohl(file_size);
    
    // Abrir archivo para escritura
    archivo.open(state.filename, std::ios::binary);
    if (!archivo) {
        printf("Error creando archivo\n");
        close(socket);
        return -1;
    }
    
    // Recibir archivo con handoff
    while (state.bytes_received < state.total_size) {
        // Verificar mensajes de control
        if (is_control_message(socket)) {
            ControlMessage msg;
            if (receive_control_message(socket, &msg) < 0) {
                break;
            }
            
            if (msg.type == MSG_SWITCH_TO_TCP) {
                // Cambiar a TCP
                if (send_control_message(socket, MSG_PROTOCOL_READY, 0) < 0) {
                    break;
                }
                
                archivo.close();
                close(socket);
                
                socket = setup_tcp_server();
                if (socket < 0) break;
                
                // Reabrir archivo en posición correcta
                archivo.open(state.filename, std::ios::binary | std::ios::in | std::ios::out);
                archivo.seekp(state.bytes_received);
                
                state.current_protocol = PROTOCOL_TCP;
                continue;
                
            } else if (msg.type == MSG_SWITCH_TO_UDP) {
                // Cambiar a UDP
                if (send_control_message(socket, MSG_PROTOCOL_READY, 0) < 0) {
                    break;
                }
                
                archivo.close();
                close(socket);
                
                socket = setup_udp_server();
                if (socket < 0) break;
                
                // Reabrir archivo en posición correcta
                archivo.open(state.filename, std::ios::binary | std::ios::in | std::ios::out);
                archivo.seekp(state.bytes_received);
                
                state.current_protocol = PROTOCOL_UDP;
                continue;
            }
        }
        
        // Recibir datos
        size_t bytes_to_receive = std::min((size_t)BUFFER_SIZE, state.total_size - state.bytes_received);
        ssize_t result = recv(socket, buffer, bytes_to_receive, 0);
        
        if (result <= 0) {
            printf("Error recibiendo datos\n");
            break;
        }
        
        archivo.write(buffer, result);
        state.bytes_received += result;
        state.packet_count++;
    }
    
    archivo.close();
    close(socket);
    return 0;
}

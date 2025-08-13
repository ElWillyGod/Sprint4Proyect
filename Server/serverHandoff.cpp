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
    // El header ya fue leído en el bucle principal
    if (recv(socket, msg, sizeof(*msg), 0) != sizeof(*msg)) {
        printf("Error recibiendo mensaje de control\n");
        return -1;
    }
    
    msg->type = ntohl(msg->type);
    msg->resume_position = ntohl(msg->resume_position);
    return 0;
}

int server_with_handoff() {
    TransferState state;
    state.bytes_received = 0;
    state.current_protocol = PROTOCOL_UDP;
    state.packet_count = 0;
    state.total_size = 0;
    
    int socket;
    char buffer[BUFFER_SIZE];
    std::ofstream archivo;
    
    // Inicializar con UDP
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
    
    // Abrir archivo para escritura
    archivo.open(state.filename, std::ios::binary);
    if (!archivo) {
        printf("Error creando archivo\n");
        close(socket);
        return -1;
    }
    
    // Recibir archivo con handoff usando headers
    while (state.bytes_received < state.total_size) {
        MessageHeader header;
        
        // Recibir header del mensaje
        if (recv(socket, &header, sizeof(header), 0) != sizeof(header)) {
            printf("Error recibiendo header\n");
            break;
        }
        
        header.magic = ntohl(header.magic);
        header.type = ntohl(header.type);
        header.length = ntohl(header.length);
        
        if (header.magic == MAGIC_CONTROL) {
            // Es un mensaje de control
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
            
        } else if (header.magic == MAGIC_DATA) {
            // Son datos del archivo
            size_t bytes_to_receive = std::min(header.length, (uint32_t)(state.total_size - state.bytes_received));
            
            if (bytes_to_receive > BUFFER_SIZE) {
                printf("Error: chunk demasiado grande\n");
                break;
            }
            
            ssize_t result = recv(socket, buffer, bytes_to_receive, 0);
            if (result <= 0) {
                printf("Error recibiendo datos\n");
                break;
            }
            
            archivo.write(buffer, result);
            archivo.flush();
            state.bytes_received += result;
            state.packet_count++;
            
        } else {
            printf("Error: magic number inválido\n");
            break;
        }
    }
    
    archivo.close();
    close(socket);
    return 0;
}

#include "client.h"

    /*
    socklen_t optlen = sizeof(socket_type);
    getsockopt(socket, SOL_SOCKET, SO_TYPE, &socket_type, &optlen);

    while (total_sent < length) {
        if (socket_type == SOCK_DGRAM) {
            ssize_t bytes_sent = sendto(socket, (char *)buffer + total_sent, length - total_sent, 0, NULL, 0);
            if (bytes_sent <= 0) {
                return false;
            }
            total_sent += bytes_sent;
        } else {
            ssize_t bytes_sent = send(socket, (char *)buffer + total_sent, length - total_sent, 0);
            if (bytes_sent <= 0) {
                return false;
            }
            total_sent += bytes_sent;
        }
    }
    return true;
    */

/// tener en cuenta el uso de getsockopt para determinar el tipo de socket, getpeername y setsockopt
bool sendAll(int socket, void *buffer, size_t length) {
    size_t total_sent = 0;
    int socket_type;

    while (total_sent < length) {
        ssize_t bytes_sent = send(socket, (char *)buffer + total_sent, length - total_sent, 0);
        if (bytes_sent <= 0) {
            return false; // Error al enviar
        }
        total_sent += bytes_sent;
    }
    return true; // Envío exitoso
    
}

int sendFile(int sockfd, std::string &filename) {

    char buffer[BUFFER_SIZE];
    std::ifstream archivo(filename, std::ios::binary);
    if (!archivo) {
        perror("No se pudo abrir el archivo");
        return -1;
    }

    // Enviar longitud del nombre del archivo
    size_t filename_length = htonl(filename.length());
    if (!sendAll(sockfd, &filename_length, sizeof(filename_length))) {
        perror("Error al enviar longitud del nombre del archivo");
        return -1;
    }

    // Enviar nombre del archivo
    if (!sendAll(sockfd, filename.data(), filename.length())) {
        perror("Error al enviar nombre del archivo");
        return -1;
    }

    // Enviar tamaño del archivo
    archivo.seekg(0, std::ios::end);
    size_t file_size = archivo.tellg();
    archivo.seekg(0, std::ios::beg);
    file_size = htonl(file_size);
    if (!sendAll(sockfd, &file_size, sizeof(file_size))) {
        perror("Error al enviar tamaño del archivo");
        return -1;
    }

    // Enviar contenido del archivo
    // Aca tiene que estar el cambio de protocolo??
    while (!archivo.eof()) {
        archivo.read(buffer, sizeof(buffer));
        send(sockfd, buffer, archivo.gcount(), 0);
    }

    archivo.close();
    return 0; // Envío exitoso
}


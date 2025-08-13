#include "server.h"

bool recvAll(int socket, void *buffer, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        ssize_t bytes_received = recv(socket, (char *)buffer + total_received, length - total_received, 0);
        if (bytes_received <= 0) {
            return false;
        }
        total_received += bytes_received;
    }
    return true;
}

int recvFile(int server_fd) {
    char buffer[BUFFER_SIZE];


    size_t filename_length;
    if (!recvAll(server_fd, &filename_length, sizeof(filename_length))) {
        printf("Error al recibir longitud del nombre del archivo\n");
        close(server_fd);
        close(server_fd);
        return -1;
    }

    filename_length = ntohl(filename_length);

    // nombre del archivo
    char* temp_filename = new char[filename_length + 1];
    if (!recvAll(server_fd, temp_filename, filename_length)) {
        printf("Error al recibir nombre del archivo\n");
        delete[] temp_filename;
        close(server_fd);
        close(server_fd);
        return -1;
    }
    temp_filename[filename_length] = '\0';
    std::string filename(temp_filename);
    delete[] temp_filename;

    // tamanio del archivo

    size_t file_size;
    if (!recvAll(server_fd, &file_size, sizeof(file_size))) {
        printf("Error al recibir tamaño del archivo\n");
        close(server_fd);
        close(server_fd);
        return -1;
    }

    file_size = ntohl(file_size);

    // crear archivo

    std::ofstream archivo(filename, std::ios::binary);
    if (!archivo) {
        printf("Error al crear archivo\n");
        close(server_fd);
        close(server_fd);
        return -1;
    }

    // recibir contenido del archivo

    size_t bytes_received = 0;
    while (bytes_received < file_size) {
        size_t bytes_to_receive = std::min(sizeof(buffer), file_size - bytes_received);
        ssize_t result = recv(server_fd, buffer, bytes_to_receive, 0);

        if (result < 0) {
            printf("Error al recibir contenido del archivo\n");
            archivo.close();
            close(server_fd);
            close(server_fd);
            return -1;
        }
        
        archivo.write(buffer, result);
        bytes_received += result;
    }

    printf("se supone que anda bien\n");

    archivo.close();

    return 0;
}

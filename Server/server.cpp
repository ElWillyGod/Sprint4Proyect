#include "server.h"

int main() {
    int server_fd, cliente;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    if(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0){
        printf("Dirección IP no válida\n");
        return -1;
    }

    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error al hacer bind");
        return -1;
    }


    // Escuchar y esperar conexion
    listen(server_fd, 3);
    cliente = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if(cliente < 0) {
        printf("Error al aceptar conexión\n");
        return -1;
    }
    // de aca todo se complica

    // longitud del nombre del archivo

    size_t filename_length;
    if (!recvAll(cliente, &filename_length, sizeof(filename_length))) {
        printf("Error al recibir longitud del nombre del archivo\n");
        close(cliente);
        close(server_fd);
        return -1;
    }

    filename_length = ntohl(filename_length);

    // nombre del archivo

    std::string filename(filename_length, '\0');
    if (!recvAll(cliente, &filename[0], filename_length)) {
        printf("Error al recibir nombre del archivo\n");
        close(cliente);
        close(server_fd);
        return -1;
    }

    // tamanio del archivo

    size_t file_size;
    if (!recvAll(cliente, &file_size, sizeof(file_size))) {
        printf("Error al recibir tamaño del archivo\n");
        close(cliente);
        close(server_fd);
        return -1;
    }

    file_size = ntohl(file_size);

    // crear archivo

    std::ofstream archivo(filename, std::ios::binary);
    if (!archivo) {
        printf("Error al crear archivo\n");
        close(cliente);
        close(server_fd);
        return -1;
    }

    // recibir contenido del archivo

    size_t bytes_received = 0;
    while (bytes_received < file_size) {
        size_t bytes_to_receive = std::min(sizeof(buffer), file_size - bytes_received);
        ssize_t result = recv(cliente, buffer, bytes_to_receive, 0);
        if (result < 0) {
            printf("Error al recibir contenido del archivo\n");
            archivo.close();
            close(cliente);
            close(server_fd);
            return -1;
        }
        archivo.write(buffer, result);
        bytes_received += result;
    }

    printf("se supone que anda bien\n");

    archivo.close();
    close(cliente);
    close(server_fd);
    return 0;
}
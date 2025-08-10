/*
    Primero vamos a hacer un envio de archivos, mediante un socket TCP
*/

#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9110

int main() {
    int server_fd, cliente;
    struct sockaddr_in address;
    char buffer[1024];

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
    int addrlen = sizeof(address);

    listen(server_fd, 3);
    cliente = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

    if(cliente < 0) {
        printf("Error al aceptar conexión\n");
        return -1;
    }

    std::ofstream archivo("archivo_recibido.txt", std::ios::binary);
    if (!archivo) {
        printf("No hay archivo\n");
        close(cliente);
        close(server_fd);
        return -1;
    }

    ssize_t bytes_read;
    while ((bytes_read = recv(cliente, buffer, sizeof(buffer), 0)) > 0) {
        archivo.write(buffer, bytes_read);
    }

    printf("se supone que anda bien\n");

    archivo.close();
    close(cliente);
    close(server_fd);
    return 0;
}
#include "server.h"

int server_tcp(){

    int server_fd, cliente;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

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

   if(recvFile(cliente) < 0) {
        printf("Error al recibir archivo\n");
        close(cliente);
        close(server_fd);
        return -1;

    }

    close(server_fd);
    return 0;
}

#include "server.h"

int server_udp(){
    int server_fd;
    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

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

    if(recvFile(server_fd) < 0) {
        printf("Error al recibir archivo\n");
        close(server_fd);
        return -1;
    }


    close(server_fd);
    return 0;
}
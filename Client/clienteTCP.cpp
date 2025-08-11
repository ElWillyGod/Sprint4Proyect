#include "client.h"

int client_tcp() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    std::string filename = "archivoDePrueba.txt";

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("Error al crear socket");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Dirección IP no válida");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error al conectar");
        return -1;
    }
    
    //////////////////////////////////////////////////////
    if(sendFile(sockfd, filename) < 0) {
        perror("Error al enviar archivo");
        close(sockfd);
        return -1;
    }
    
    close(sockfd);
    return 0;
}
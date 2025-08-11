#include "client.h"

int client_udp(){
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr;
    std::string filename = "archivoDePrueba.txt";

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sockfd < 0) {
        perror("Error al crear socket");
        return -1;
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0) {
        perror("Dirección IP no válida");
        return -1;
    }

    // vamos a hacer el connect para no tener que usar sendto y poder usar send igual que en TCP

    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
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
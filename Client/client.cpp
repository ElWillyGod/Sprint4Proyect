#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9110

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[1024];

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

    std::ifstream archivo("archivoDePrueba.txt", std::ios::binary);
    if (!archivo) {
        perror("No se pudo abrir el archivo");
        return -1;
    }

    while (archivo.read(buffer, sizeof(buffer))) {
        send(sockfd, buffer, archivo.gcount(), 0);
    }

    archivo.close();
    close(sockfd);
    return 0;
}
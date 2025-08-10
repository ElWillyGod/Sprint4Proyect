#include "client.h"

int main() {
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
    while (!archivo.eof()) {
        archivo.read(buffer, sizeof(buffer));
        send(sockfd, buffer, archivo.gcount(), 0);
    }

    archivo.close();
    close(sockfd);
    return 0;
}
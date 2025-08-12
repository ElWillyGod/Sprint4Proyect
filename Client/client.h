#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>

#define PORT 9110
#define BUFFER_SIZE 1024

struct TransferState {
    std::string filename;
    size_t total_size;
    size_t bytes_sent;
    int current_protocol;
    int packet_count;

    std::vector<bool> packet_received; 
};

/////////////////////////////////////////////////////
// son las lo mismo pero cambiando el tipo de socket y protocolo
int client_tcp();

int client_udp();
/////////////////////////////////////////////////////


bool sendAll(int socket, void *buffer, size_t length);
int sendFile(int sockfd, std::string &filename);

#endif // CLIENT_H
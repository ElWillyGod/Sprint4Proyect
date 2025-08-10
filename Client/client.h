#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9110
#define BUFFER_SIZE 1024

bool sendAll(int socket, void *buffer, size_t length);

#endif // CLIENT_H
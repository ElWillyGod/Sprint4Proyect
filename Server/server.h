#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9110
#define BUFFER_SIZE 1024

#define PROTOCOL_UDP 0
#define PROTOCOL_TCP 1

#define MSG_SWITCH_TO_TCP 1
#define MSG_SWITCH_TO_UDP 2
#define MSG_RESUME_TRANSFER 3
#define MSG_PROTOCOL_READY 4

struct TransferState {
    std::string filename;
    size_t total_size;
    size_t bytes_received;
    int current_protocol;
    int packet_count;
};

struct ControlMessage {
    int type;
    size_t resume_position;
};

/////////////////////////////////////////////////////
int server_tcp();

/////////////////////////////////////////////////////
int server_udp();

// Handoff functions
int server_with_handoff();
int setup_tcp_server();
int setup_udp_server();
int send_control_message(int socket, int msg_type, size_t resume_pos);
int receive_control_message(int socket, ControlMessage* msg);
bool is_control_message(int socket);

bool recvAll(int socket, void *buffer, size_t length);

int recvFile(int server_fd);

#endif // SERVER_H
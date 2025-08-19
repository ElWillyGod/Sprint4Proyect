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

#define PROTOCOL_UDP 0
#define PROTOCOL_TCP 1

#define MSG_SWITCH_TO_TCP 1
#define MSG_SWITCH_TO_UDP 2
#define MSG_RESUME_TRANSFER 3
#define MSG_PROTOCOL_READY 4

#define MAGIC_CONTROL 0xDEADBEEF
#define MSG_CONTROL 1

typedef struct {
    uint32_t magic;      // MAGIC_CONTROL
    uint32_t type;       // MSG_CONTROL
    uint32_t length;     // tamaño del payload
} MessageHeader;

struct TransferState {
    std::string filename;
    size_t total_size;
    size_t bytes_sent;
    int current_protocol;
    int packet_count;
};

struct ControlMessage {
    int type;
    size_t resume_position;
};

// Handoff functions
int client_with_handoff();
bool should_switch_protocol(TransferState* state, int socket);
int switch_protocol(TransferState* state, int* socket);
int setup_tcp_connection();
int setup_udp_connection();
int send_control_message(int socket, int msg_type, size_t resume_pos);
int receive_control_message(int socket, ControlMessage* msg);

// Utility functions
bool sendAll(int socket, const void *buffer, size_t length);

#endif // CLIENT_H
#include "client.h"

bool sendAll(int socket, void *buffer, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t bytes_sent = send(socket, (char *)buffer + total_sent, length - total_sent, 0);
        if (bytes_sent <= 0) {
            return false;
        }
        total_sent += bytes_sent;
    }
    return true;
}


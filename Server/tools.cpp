#include "server.h"

bool recvAll(int socket, void *buffer, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        ssize_t bytes_received = recv(socket, (char *)buffer + total_received, length - total_received, 0);
        if (bytes_received <= 0) {
            return false;
        }
        total_received += bytes_received;
    }
    return true;
}

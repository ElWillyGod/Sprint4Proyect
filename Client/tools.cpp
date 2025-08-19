#include "client.h"
bool sendAll(int socket, const void *buffer, size_t length) {
    size_t total_sent = 0;

    while (total_sent < length) {
        ssize_t bytes_sent = send(socket, (const char *)buffer + total_sent, length - total_sent, 0);
        if (bytes_sent <= 0) {
            return false; // Error al enviar
        }
        total_sent += bytes_sent;
    }
    return true; // Envío exitoso
}


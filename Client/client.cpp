#include "client.h"

// El cliente TCP y UDP son iguales, solo cambia el tipo de socket, eliminar.

int main() {
   // return client_udp();
   return client_with_handoff();
}
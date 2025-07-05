// file:        network.h
// description: definitions of network useful functions
#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>

int parse_addr(const char *addr_str, struct sockaddr_storage *storage);
int set_server_socket(const char *protocol);
int send_packet(int fd, const char *msg, size_t msg_size);

#endif
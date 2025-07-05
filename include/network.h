// file:        network.h
// description: definitions of network useful functions
#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include "p2p-net.h"

void init_server(P2PNet *pn);
int connect_to_peer(const char *addr_str, P2PNet *pn);

#endif
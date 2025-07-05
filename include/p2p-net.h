// file:        operations.h
// description: definitions of program main operations
#ifndef P2P_NET_H
#define P2P_NET_H

#include "defs.h"
#include <pthread.h>

typedef struct {
    int sock_fd;
    Peer peers[MAX_PEERS];
    int peer_count;
    ChatMessage *chat_history;
    int history_size;
    pthread_mutex_t net_mutex;
    pthread_mutex_t chat_mutex;
} P2PNet;

// global variable
extern P2PNet p2p_net;

void init_p2p_net(P2PNet *pn);
void add_peer_to_p2p_net(P2PNet *pn, int fd, uint32_t ip);
void remove_peer_from_p2p_net(P2PNet *pn, int fd);

#endif
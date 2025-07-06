// file:        operations.h
// description: definitions of program main operations
#ifndef P2P_NET_H
#define P2P_NET_H

#include "defs.h"
#include <pthread.h>

typedef struct {
    int running;
    int sock_fd;
    Peer peers[MAX_PEERS];
    int threads_count;
    pthread_t peer_threads[MAX_PEERS];
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
int send_peer_request(int fd);
int send_peer_list(int fd, P2PNet *pn);
int handle_peer_list(int fd,  P2PNet *pn);
int send_archive(int fd, P2PNet *pn);
int handle_archive(int fd, P2PNet *pn);
int handle_notification(int fd);
void list_peers(P2PNet *pn);
void list_history(P2PNet *pn);
void send_chat_message(P2PNet *pn, const char *message);
void clean_p2p_net(P2PNet *pn);

#endif
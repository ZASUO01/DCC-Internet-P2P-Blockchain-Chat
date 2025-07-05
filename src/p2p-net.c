#include "p2p-net.h"
#include "network.h"
#include "logger.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

// global variable
P2PNet p2p_net;

// server initializer
void init_p2p_net(P2PNet *pn){
  pn->sock_fd = -1;
  pn->chat_history = NULL;
  pn->history_size = 0;
  pn->peer_count = 0;
  pthread_mutex_init(&pn->net_mutex, NULL);
  pthread_mutex_init(&pn->chat_mutex, NULL);
}

// add to the peers list
void add_peer_to_p2p_net(P2PNet *pn, int fd, uint32_t ip){
  pthread_mutex_lock(&pn->net_mutex);
  pn->peers[pn->peer_count].socket = fd;
  pn->peers[pn->peer_count].ip = ip;
  pn->peer_count++;
  pthread_mutex_unlock(&pn->net_mutex);
}

// remove a peer from list by sock fd
void remove_peer_from_p2p_net(P2PNet *pn, int fd){
   pthread_mutex_lock(&pn->net_mutex);
    
    for (int i = 0; i < pn->peer_count; i++) {
        if (pn->peers[i].socket == fd) {

            // Move the last element to the removed position
            if (i < pn->peer_count - 1) {
                pn->peers[i] = pn->peers[pn->peer_count - 1];
            }
            
            pn->peer_count--;
            
            //socket found and removed
            break; 
        }
    }
    
    pthread_mutex_unlock(&pn->net_mutex);
}
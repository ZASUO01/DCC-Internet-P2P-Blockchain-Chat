#include "defs.h"
#include "network.h"
#include "logger.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// try to parse ipv4 or ipv6 addr
static int parse_addr(const char *addr_str, struct sockaddr_in *addr4){
  if(addr_str == NULL){
    return -1;
  }

  // set the port
  uint16_t port = htons(PORT);

  // try to parse IPv4
  struct in_addr in_addr4;
  if (inet_pton(AF_INET, addr_str, &in_addr4)) {
    addr4->sin_addr = in_addr4;
    addr4->sin_port = port;
    addr4->sin_family = AF_INET;
    return 0;
  }

  return -1;
}



// init server socket and bind ip
void init_server(P2PNet *pn){
   // try to init server addr
  struct sockaddr_in addr4;
  addr4.sin_addr.s_addr = INADDR_ANY;
  addr4.sin_port = htons(PORT);
  addr4.sin_family = AF_INET;
  struct sockaddr *addr = (struct sockaddr *)&addr4;

  // server tcp socket
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(sock_fd < 0){
    log_exit("server socket creation failure");
  }

  // try to bind socket
  if (bind(sock_fd, addr, sizeof(addr4)) != 0) {
    close(sock_fd);
    log_exit("bind failure");
  }

  // try to linsten on socket
  if (listen(sock_fd, 10) != 0) {
    close(sock_fd);
    log_exit("listen failure");
  }

  // store the socket fd
  pn->sock_fd = sock_fd;
}

// connect to a peer and store it in the peers list
int connect_to_peer(const char *addr_str, P2PNet *pn){
  struct sockaddr_in addr4;
  if(parse_addr(addr_str, &addr4) != 0){
    pthread_mutex_lock(&pn->net_mutex);
    close(pn->sock_fd);
    pthread_mutex_unlock(&pn->net_mutex);
    return -1;
  }
  struct sockaddr *addr = (struct sockaddr *)&addr4;

  int known_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(known_fd < 0){  
    pthread_mutex_lock(&pn->net_mutex);
    close(pn->sock_fd);
    pthread_mutex_unlock(&pn->net_mutex);
    return -1;
  }

  if(connect(known_fd, addr, sizeof(addr4)) != 0){
    pthread_mutex_lock(&pn->net_mutex);
    close(pn->sock_fd);
    pthread_mutex_unlock(&pn->net_mutex);
    close(known_fd);
    return -1;
  }

   add_peer_to_p2p_net(pn, known_fd, addr4.sin_addr.s_addr);
   return known_fd;
}

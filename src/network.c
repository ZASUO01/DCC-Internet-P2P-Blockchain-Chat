#include "defs.h"
#include "network.h"
#include "logger.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// try to parse ipv4 or ipv6 addr
int parse_addr(const char *addr_str, struct sockaddr_storage *storage) {
  if (addr_str == NULL) {
    return -1;
  }

  // set the port
  uint16_t port  = htons(PORT);

  // try to parse IPv4
  struct in_addr in_addr4;
  if (inet_pton(AF_INET, addr_str, &in_addr4)) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
    addr4->sin_addr = in_addr4;
    addr4->sin_port = port;
    addr4->sin_family = AF_INET;
    return 0;
  }

  // try to parse IPv6
  struct in6_addr in_addr6;
  if (inet_pton(AF_INET6, addr_str, &in_addr6)) {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
    memcpy(&(addr6->sin6_addr), &in_addr6, sizeof(in_addr6));
    addr6->sin6_port = port;
    addr6->sin6_family = AF_INET6;
    return 0;
  }

  return -1;
}

// initialize the server ip addr structure
static int init_server_addr(const char *protocol, struct sockaddr_storage *storage){
  // can't parse null strings
  if (protocol == NULL) {
    return -1;
  }

  // set the port
  uint16_t port = htons(PORT);

  // get the addr for the given protocol
  if (strcmp(protocol, "v4") == 0) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
    addr4->sin_addr.s_addr = INADDR_ANY;
    addr4->sin_port = port;
    addr4->sin_family = AF_INET;
    return 0;
  } else if (strcmp(protocol, "v6") == 0) {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
    addr6->sin6_addr = in6addr_any;
    addr6->sin6_port = port;
    addr6->sin6_family = AF_INET6;
    return 0;
  }

  return -1;
}

// init this server socket
// protocol can be v4 or v6
int set_server_socket(const char *protocol) {
   // try to init server addr
  struct sockaddr_storage storage;
  if(init_server_addr(protocol, &storage) != 0){
    log_exit("server addr init failure");
  }
  struct sockaddr *addr = (struct sockaddr *)&storage;

  // server tcp socket
  int sock_fd = socket(storage.ss_family, SOCK_STREAM, 0);
  if(sock_fd < 0){
    log_exit("server socket creation failure");
  }

  // try to bind socket
  if (bind(sock_fd, addr, sizeof(storage)) != 0) {
    log_exit("bind failure");
  }

  // try to linsten on socket
  if (listen(sock_fd, 10) != 0) {
    log_exit("listen failure");
  }

  return sock_fd;
}

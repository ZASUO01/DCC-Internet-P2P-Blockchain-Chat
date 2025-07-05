// file:        defs.h
// description: definitions of program general data and structures
#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

#define PORT 51511
#define MAX_IP 64
#define MAX_PEERS 1000
#define PEER_TIMEOUT_SEC 30
#define MAX_INPUT 256
#define MAX_CHAT_LEN 256
#define VERIFICATION_CODE_SIZE 16
#define MD5_HASH_SIZE 16

#define PEER_REQUEST 0x1
#define PEER_LIST 0x2
#define ARCHIVE_REQUEST 0x3
#define ARCHIVE_RESPONSE 0x4
#define NOTIFICATION 0x5

typedef struct {
    uint32_t ip;
    int socket;
} Peer;

typedef struct {
    uint8_t length;
    char message[MAX_CHAT_LEN];
    uint8_t verification_code[VERIFICATION_CODE_SIZE];
    uint8_t md5_hash[MD5_HASH_SIZE];
} ChatMessage;

#endif
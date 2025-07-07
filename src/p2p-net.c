#include "p2p-net.h"
#include "network.h"
#include "logger.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <openssl/evp.h>

// global variable
P2PNet p2p_net;

// LOCAL FUNCTIONS ------------------------------------------------

// mine end add the message to the chat history
static int mine_and_add_chat(P2PNet *pn, const char *message, size_t msg_size){
    // prepare the new chat
    ChatMessage new_chat;
    new_chat.length = (uint8_t)msg_size;
    memcpy(new_chat.message, message, msg_size);

    // get the last 19 chats to make the S sequence
    pthread_mutex_lock(&pn->chat_mutex);
    int history_count = pn->history_size;
    int start_index = (history_count > 19) ? history_count - 19 : 0;

    // find the verification code
    srand(time(NULL));

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        pthread_mutex_unlock(&pn->chat_mutex);
        return -1;
    }

    int found = 0;
    unsigned char calculated_hash[MD5_HASH_SIZE];

    while (!found) {
        // generate random verification code
        for (int i = 0; i < VERIFICATION_CODE_SIZE; i++) {
            new_chat.verification_code[i] = rand() % 256;
        }

        // calculate the MD5 hash of S sequence
        // init
        if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
            EVP_MD_CTX_free(mdctx);
            pthread_mutex_unlock(&pn->chat_mutex);
            return -1;
        }

        // Add the last 19 chats or less
        for (int i = start_index; i < history_count; i++) {
            EVP_DigestUpdate(mdctx, &pn->chat_history[i].length, 1);
            EVP_DigestUpdate(mdctx, pn->chat_history[i].message, pn->chat_history[i].length);
            EVP_DigestUpdate(mdctx, pn->chat_history[i].verification_code, VERIFICATION_CODE_SIZE);
            EVP_DigestUpdate(mdctx, pn->chat_history[i].md5_hash, MD5_HASH_SIZE);
        }

        // add the new chat whithout the md5 hash
        EVP_DigestUpdate(mdctx, &new_chat.length, 1);
        EVP_DigestUpdate(mdctx, new_chat.message, new_chat.length);
        EVP_DigestUpdate(mdctx, new_chat.verification_code, VERIFICATION_CODE_SIZE);

        unsigned int md_len;
        if (EVP_DigestFinal_ex(mdctx, calculated_hash, &md_len) != 1 || md_len != MD5_HASH_SIZE) {
            EVP_MD_CTX_free(mdctx);
            pthread_mutex_unlock(&pn->chat_mutex);
            return -1;
        }

        // verify if the first two bytes are zero
        if (calculated_hash[0] == 0 && calculated_hash[1] == 0) {
            found = 1;
            memcpy(new_chat.md5_hash, calculated_hash, MD5_HASH_SIZE);
        }
    }

    EVP_MD_CTX_free(mdctx);

    // add the new chat to the history
    ChatMessage *new_history = realloc(pn->chat_history, (history_count + 1) * sizeof(ChatMessage));
    if (!new_history) {
        pthread_mutex_unlock(&pn->chat_mutex);
        return -1;
    }

    pn->chat_history = new_history;
    memcpy(&pn->chat_history[history_count], &new_chat, sizeof(ChatMessage));
    pn->history_size++;

    pthread_mutex_unlock(&pn->chat_mutex);
    return 0;
}


// check if the chat is valid
static int is_valid_chat(ChatMessage *history, int count){
    if(count == 0 ){
        return 1;
    }

    // Check the last chat
    ChatMessage *last = &history[count-1];

    // check if the last chat starts with 00
    if (last->md5_hash[0] != 0 || last->md5_hash[1] != 0) {
        return 0;
    }

    // compute the md5 hash of the last 20 chats or the last remaining
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return 0;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        return 0;
    }
 
    int start = (count > 20) ? count - 20 : 0;
    for (int i = start; i < count; i++) {
        ChatMessage *chat = &history[i];
        
        if (i < count-1) {
            // previous chats, include all
            EVP_DigestUpdate(mdctx, &chat->length, 1);
            EVP_DigestUpdate(mdctx, chat->message, chat->length);
            EVP_DigestUpdate(mdctx, chat->verification_code, VERIFICATION_CODE_SIZE);
            EVP_DigestUpdate(mdctx, chat->md5_hash, MD5_HASH_SIZE);
        } else {
            // last chat, exclude the own md5
            EVP_DigestUpdate(mdctx, &chat->length, 1);
            EVP_DigestUpdate(mdctx, chat->message, chat->length);
            EVP_DigestUpdate(mdctx, chat->verification_code, VERIFICATION_CODE_SIZE);
        }
    }
    
    // get the calculated hash
    unsigned char calculated_hash[MD5_HASH_SIZE];
    unsigned int md_len;
    if (EVP_DigestFinal_ex(mdctx, calculated_hash, &md_len) != 1 || md_len != MD5_HASH_SIZE) {
        EVP_MD_CTX_free(mdctx);
        return 0;
    }
    EVP_MD_CTX_free(mdctx);
    
    // Compare the stored hash
    if (memcmp(calculated_hash, last->md5_hash, MD5_HASH_SIZE) != 0) {
        return 0;
    }
    
    // Recursive verification of the history withouth the last chat;
    return is_valid_chat(history, count-1);
}


// generic sender function
static int send_bytes(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    
    while (sent < len) {
        ssize_t bytes = send(fd, (char*)buf + sent, len - sent, 0);
        if (bytes <= 0){
            return -1;
        } 
        sent += bytes;
    }
    
    return 0;
}


// MAIN FUNCTIONS ---------------------------------------------------

// server initializer
void init_p2p_net(P2PNet *pn){
    pn->threads_count = 0;
    pn->running = 1;
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


// send the request of the peer list
int send_peer_request(int fd){
    uint8_t msg = PEER_REQUEST;

    if(send_bytes(fd, &msg, 1) != 0){
        return -1;
    }

    return 0;
}


// send the list of peers known
int send_peer_list(int fd, P2PNet *pn){
    uint8_t msg_type = PEER_LIST;
    pthread_mutex_lock(&pn->net_mutex);
    
    uint32_t peer_count = htonl(pn->peer_count);
    size_t buff_size = 5 + (pn->peer_count * 4);
    uint8_t buff[buff_size];

    memcpy(buff, &msg_type, 1);
    memcpy(buff + 1, &peer_count, 4);

    size_t offset = 5;
    for(int i = 0; i < pn->peer_count; i++){
        offset += i * 4;
        uint32_t ip = htonl(pn->peers[i].ip);
        memcpy(buff + offset, &ip, 4);        
    }

    pthread_mutex_unlock(&pn->net_mutex);
    
    if(send_bytes(fd, buff, buff_size) != 0){
        return -1;
    }

    return 0;
}


// handle the receipt of a peer list
int handle_peer_list(int fd,  P2PNet *pn){
    uint32_t peer_count;

    // receive the peer count first
    ssize_t bytes = recv(fd, &peer_count, 4, MSG_WAITALL); 
    if (bytes != 4) {
        return -1;
    }
    peer_count = ntohl(peer_count);

    // receive the ip list then
    uint32_t ips[MAX_PEERS];
    size_t ips_size = peer_count * 4;
    bytes = recv(fd, ips, ips_size, MSG_WAITALL);
    if (bytes != (ssize_t)ips_size) {
        return -1;
    }

    // connect to peers
    for(uint32_t i = 0; i < peer_count; i++){
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ips[i], ip_str, INET_ADDRSTRLEN);

        // ADD PEER
    }

    return 0;
}


// send the chat history to the peer
int send_archive(int fd, P2PNet *pn){
    uint8_t response_type = ARCHIVE_RESPONSE;

    pthread_mutex_lock(&p2p_net.chat_mutex);
    uint32_t chat_count = htonl(pn->history_size);
    size_t total_size = 5;

    // get the size of all chats
    for (int i = 0; i < pn->history_size; i++) {
        total_size += 1 + pn->chat_history[i].length + VERIFICATION_CODE_SIZE + MD5_HASH_SIZE;
    }

    // allocate the buffer
    uint8_t *buff = malloc(total_size);
    if (!buff) {
        pthread_mutex_unlock(&pn->chat_mutex);
        return -1;
    }

    //header
    size_t offset = 0;
    buff[offset] = response_type;
    offset++;
    memcpy(buff + offset, &chat_count, 4);
    offset += 4;

    // chats data
    for (int i = 0; i < pn->history_size; i++) {
        ChatMessage *chat = &pn->chat_history[i];
        
        // msg lenght
        buff[offset] = chat->length;
        offset++;
        memcpy(buff + offset, chat->message, chat->length);
        offset += chat->length;
        
        // verification code
        memcpy(buff + offset, chat->verification_code, VERIFICATION_CODE_SIZE);
        offset += VERIFICATION_CODE_SIZE;
        
        // MD5 hash
        memcpy(buff + offset, chat->md5_hash, MD5_HASH_SIZE);
        offset += MD5_HASH_SIZE;
    }

    pthread_mutex_unlock(&p2p_net.chat_mutex);

    if(send_bytes(fd, buff, total_size) != 0){
        return -1;
    }

    free(buff);
    return 0;
}


// handle the chat history receipt
int handle_archive(int fd, P2PNet *pn){
    uint32_t chat_count;
    
    // receive chat count
    ssize_t bytes = recv(fd, &chat_count, 4, MSG_WAITALL); 
    if (bytes != 4) {
        return -1;
    }
    chat_count = ntohl(chat_count);
    
    // buffer to receive the chats
    ChatMessage *new_chats = malloc(chat_count * sizeof(ChatMessage));
    if (!new_chats){
         return -1;
    }
    
    // receive each chat
    for (uint32_t i = 0; i < chat_count; i++) {
        ChatMessage *chat = &new_chats[i];
        
        // receive the lenght
        bytes = recv(fd, &chat->length, 1, MSG_WAITALL); 
        if (bytes != 1) {
            free(new_chats);
            return -1;
        }
        
        // receive the message
        bytes = recv(fd, chat->message, chat->length, MSG_WAITALL); 
        if (bytes != (ssize_t)chat->length) {
            free(new_chats);
            return -1;
        }
        
        // receive the verification code
        bytes = recv(fd, chat->verification_code, VERIFICATION_CODE_SIZE, MSG_WAITALL); 
        if (bytes != VERIFICATION_CODE_SIZE) {
            free(new_chats);
            return -1;
        }
        
        // receive the md5 hash
        bytes = recv(fd, chat->md5_hash, MD5_HASH_SIZE, MSG_WAITALL); 
        if (bytes != MD5_HASH_SIZE) {
            free(new_chats);
            return -1;
        }
    }
    
    
    // verify if its a valid history
    if (is_valid_chat(new_chats, chat_count)) {
        pthread_mutex_lock(&pn->chat_mutex);
        
        // if the new history is bigger, replace the current
        if ((int)chat_count > pn->history_size) {
            free(pn->chat_history);
            pn->chat_history = new_chats;
            pn->history_size = chat_count;
        } else {
            free(new_chats);
        }
        
        pthread_mutex_unlock(&pn->chat_mutex);
    } else {
        free(new_chats);
        return -1;
    }
    
    return 0;
}


// handle the receipt of a notification message
int handle_notification(int fd){
    uint8_t length;
    
    // Receive the notification length
    ssize_t bytes = recv(fd, &length, 1, MSG_WAITALL); 
    if (bytes != 1){
        return -1;
    }
    
    // Receive the message then
    char message[256];
    bytes = recv(fd, message, length, MSG_WAITALL); 
    if (bytes != (ssize_t)length) {
        return -1;
    }

    message[length] = '\0';
    printf("Notification received: %s\n", message);
    return 0;
}


// print the current peers list
void list_peers(P2PNet  *pn) {
    pthread_mutex_lock(&pn->net_mutex);
    printf("Connected Peers (%d):\n", pn->peer_count);
    for (int i = 0; i < pn->peer_count; i++) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &pn->peers[i].ip, ip_str, INET_ADDRSTRLEN);
        printf("%d: %s\n", i+1, ip_str);
    }
    pthread_mutex_unlock(&pn->net_mutex);
}

// print the current chat history
void list_history(P2PNet *pn) {
    pthread_mutex_lock(&pn->chat_mutex);
    printf("Chat History (%d):\n", pn->history_size);
    for (int i = 0; i < pn->history_size; i++) {
        printf("[%d] %.*s\n", i+1, pn->chat_history[i].length, pn->chat_history[i].message);
    }
    pthread_mutex_unlock(&pn->chat_mutex);
}


// send a message to chat 
void send_chat_message(P2PNet *pn, const char *message){
    size_t msg_size = strlen(message); 
    if (msg_size > MAX_CHAT_LEN) {
        return;
    }

    // mine end add the message to the chat history
    if(mine_and_add_chat(pn, message, msg_size) != 0){
        return;
    }

    // share the history with all connected peers
    pthread_mutex_lock(&pn->net_mutex);
    for (int i = 0; i < pn->peer_count; i++) {
        send_archive(pn->peers[i].socket, pn);
    }
    pthread_mutex_unlock(&pn->net_mutex);
}


// destroy variables
void clean_p2p_net(P2PNet *pn){
    if(pn == NULL){
        return;
    }

    // disconnect to all peers
    for (int i = 0; i < pn->peer_count; i++) {
        if (pn->peers[i].socket != -1) {
            shutdown(pn->peers[i].socket, SHUT_RDWR);
            close(pn->peers[i].socket);
        }
    }
    pn->peer_count = 0;
    
    // close own socket
    shutdown(pn->sock_fd, SHUT_RDWR);
    close(pn->sock_fd);

    // remove chat history
    if(pn->chat_history != NULL){
        free(pn->chat_history);
        pn->chat_history = NULL;
        pn->history_size = 0;
    }

    // destroy mutexes
    pthread_mutex_destroy(&pn->net_mutex);
    pthread_mutex_destroy(&pn->chat_mutex);

    // reset memory
    memset(pn, 0, sizeof(P2PNet));
}
#include "operations.h"
#include "p2p-net.h"
#include "logger.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/md5.h>

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


// send the request of the peer list
static int send_peer_request(int fd){
    uint8_t msg = PEER_REQUEST;

    if(send_bytes(fd, &msg, 1) != 0){
        return -1;
    }

    return 0;
}

// sen the list of peers known
static int send_peer_list(int fd, P2PNet *pn){
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
static int handle_peer_list(int fd,  P2PNet *pn){
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

    // TODO CONNECT TO PEERS
 
    return 0;
}

// send the chat history to the peer
static int send_archive(int fd, P2PNet *pn){
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
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    
    int start = (count > 20) ? count - 20 : 0;
    for (int i = start; i < count; i++) {
        ChatMessage *chat = &history[i];
        
        if (i < count-1) {
            // previous chats, include all
            MD5_Update(&md5_ctx, &chat->length, 1);
            MD5_Update(&md5_ctx, chat->message, chat->length);
            MD5_Update(&md5_ctx, chat->verification_code, VERIFICATION_CODE_SIZE);
            MD5_Update(&md5_ctx, chat->md5_hash, MD5_HASH_SIZE);
        } else {
            // last chat, exclude the own md5
            MD5_Update(&md5_ctx, &chat->length, 1);
            MD5_Update(&md5_ctx, chat->message, chat->length);
            MD5_Update(&md5_ctx, chat->verification_code, VERIFICATION_CODE_SIZE);
        }
    }
    
    uint8_t calculated_hash[MD5_HASH_SIZE];
    MD5_Final(calculated_hash, &md5_ctx);
    
    // Compare the stored hash
    if (memcmp(calculated_hash, last->md5_hash, MD5_HASH_SIZE) != 0) {
        return 0;
    }
    
    // 3. Recursive verification of the history withouth the last chat;
    return is_valid_chat(history, count-1);
}


// handle the chat history receipt
static int handle_archive(int fd, P2PNet *pn){
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
        if (chat_count > pn->history_size) {
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


// thread to handle the communication with a peer
void *handle_peer(void *arg){
    // peer socket
    int sock_fd = *(int *)arg; 
    free(arg);

    // request the known peers list
    if(send_peer_request(sock_fd) != 0){
        shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
        return NULL;
    }

    // time control
    struct timeval last_activity;
    gettimeofday(&last_activity, NULL);

    // receive messages
    while(1){
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);

        // compute timeout
        struct timeval now, timeout;
        gettimeofday(&now, NULL);

        // check inactivity time
        long elapsed_sec = now.tv_sec - last_activity.tv_sec;
        if (elapsed_sec >= PEER_TIMEOUT_SEC) {
            break;
        }
        
        // remaining time
        timeout.tv_sec = PEER_TIMEOUT_SEC - elapsed_sec;
        timeout.tv_usec = 0;

        // check if the socket ready
        int ready = select(sock_fd + 1, &read_fds, NULL, NULL, &timeout);

        if(ready <= 0){
            break;
        }

        // process message
        uint8_t msg_type;
        ssize_t bytes = recv(sock_fd, &msg_type, 1, MSG_DONTWAIT);
        
        // failed to receive
        if(bytes <= 0){
            break;
        }

        //update last receipt time
        gettimeofday(&last_activity, NULL);

        switch(msg_type){
        case PEER_REQUEST:
            LOG_MSG(LOG_INFO, "received PEER_REQUEST");
            if(send_peer_list(sock_fd, &p2p_net) != 0){
                LOG_MSG(LOG_ERROR, "failed to send the peer list");
            }else{
                LOG_MSG(LOG_INFO, "sent peer list");
            }
            break;
        case PEER_LIST:
            LOG_MSG(LOG_INFO, "received PEER_LIST");
            if(handle_peer_list(sock_fd, &p2p_net) != 0){
                LOG_MSG(LOG_ERROR, "failed to receive peer list");
            }else{
                LOG_MSG(LOG_INFO, "peer list proccessed successfully");
            }
            break;
        case ARCHIVE_REQUEST:
            LOG_MSG(LOG_INFO, "received ARCHIVE_REQUEST");
            if(send_archive(sock_fd, &p2p_net) != 0){
                 LOG_MSG(LOG_ERROR, "failed to send the chat history");
            }else{
                LOG_MSG(LOG_INFO, "chat history sent successfully");
            }
            break;
        case ARCHIVE_RESPONSE:
             LOG_MSG(LOG_INFO, "received ARCHIVE_RESPONSE");
            if(handle_archive(sock_fd, &p2p_net) != 0){
                 LOG_MSG(LOG_ERROR, "failed to proccess the chat history");
            }else{
                LOG_MSG(LOG_INFO, "chat history proccessed successfully");
            }
            break;
        case NOTIFICATION:
            LOG_MSG(LOG_INFO, "received NOTIFICATION");
            if(handle_notification(sock_fd) != 0){
                LOG_MSG(LOG_ERROR, "failed to proccess notification");
            }else{
                LOG_MSG(LOG_INFO, "notification processed");
            }
            break;
        default:
            printf("unknown receipt\n");
            break;
        }
    }

    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    remove_peer_from_p2p_net(&p2p_net, sock_fd);

    return NULL;
}
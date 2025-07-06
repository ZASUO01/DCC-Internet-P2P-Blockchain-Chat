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
        // stop condition
        pthread_mutex_lock(&p2p_net.net_mutex);
        if(p2p_net.running == 0){
            pthread_mutex_unlock(&p2p_net.net_mutex);
            break;
        }
        pthread_mutex_unlock(&p2p_net.net_mutex);

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
            LOG_MSG(LOG_WARNING, "received UNKNOWN");
            break;
        }
    }

    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);
    remove_peer_from_p2p_net(&p2p_net, sock_fd);
    return NULL;
}



// read terminal commands
void read_inputs(){
    char input[MAX_INPUT];

    while(1){
        // stop condition
        pthread_mutex_lock(&p2p_net.net_mutex);
        if(p2p_net.running == 0){
            pthread_mutex_unlock(&p2p_net.net_mutex);
            break;
        }
        pthread_mutex_unlock(&p2p_net.net_mutex);

        // input identifier
        printf("> ");
        fflush(stdout);

        // read user input
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
           continue;
        }

        // remove newline
        input[strcspn(input, "\n")] = '\0';
        
        // can't send empty string
        if (strlen(input) == 0) {
            continue;
        }
        
        // quit
        else if (strcmp(input, "quit") == 0) {
            LOG_MSG(LOG_INFO, "read_inputs() quit");
            pthread_mutex_lock(&p2p_net.net_mutex);
            p2p_net.running = 0;
            pthread_mutex_unlock(&p2p_net.net_mutex);
            break;
        }

        // print current peers
        else if (strcmp(input, "peers") == 0) {
            list_peers(&p2p_net);
        }

        // print current chat
        else if (strcmp(input, "history") == 0) {
            list_history(&p2p_net);
        }

        // 
        else if (strncmp(input, "/connect ", 9) == 0) {
            //connect_to_peer(network, input + 9);
        }
        
        // mine hash and add message to chat
        else {
            send_chat_message(&p2p_net, input);
        }
    }
}
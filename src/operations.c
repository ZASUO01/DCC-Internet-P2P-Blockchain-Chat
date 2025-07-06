#include "operations.h"
#include "network.h"
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
#include <unistd.h>
#include <stdio.h>

// send a peer request to all the connected peers every 5 seconds
void *periodic_request(){
    while(1){
        // stop condition
        pthread_mutex_lock(&p2p_net.net_mutex);
        if(p2p_net.running == 0){
            pthread_mutex_unlock(&p2p_net.net_mutex);
            break;
        }
        
        for(int i = 0; i < p2p_net.peer_count; i++){
            send_peer_request(p2p_net.peers[i].socket);
        }
        
        pthread_mutex_unlock(&p2p_net.net_mutex);
        
        sleep(5);
    }
    return NULL;
}


// wait and accept peer connections
void *accept_connections(){
    while(1){
        // stop condition
        pthread_mutex_lock(&p2p_net.net_mutex);
        if(p2p_net.running == 0){
            pthread_mutex_unlock(&p2p_net.net_mutex);
            break;
        }

        int sock = p2p_net.sock_fd;
        pthread_mutex_unlock(&p2p_net.net_mutex);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // wait for activity in the socket
        int activity = select(sock + 1, &read_fds, NULL, NULL, &timeout);
        
        // if activity happened, accept connection
        if (activity > 0 && FD_ISSET(sock, &read_fds)) {
            struct sockaddr_in peer_addr;
            socklen_t addr_len = sizeof(peer_addr);
            int peer_fd = accept(sock, (struct sockaddr*)&peer_addr, &addr_len);
            
            if (peer_fd < 0) {
                continue;
            }

            // get ip str
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peer_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
            printf("Peer %s\n has connected to you", ip_str);

            // add the new peer to the list
            add_peer_to_p2p_net(&p2p_net, peer_fd, peer_addr.sin_addr.s_addr);
        
            // create a thread for this peer
            int * sock_t = malloc(sizeof(int));
            *sock_t = peer_fd;

            pthread_mutex_lock(&p2p_net.net_mutex);
            pthread_create(&p2p_net.peer_threads[p2p_net.threads_count], NULL, handle_peer, sock_t);
            p2p_net.threads_count++;
            pthread_mutex_unlock(&p2p_net.net_mutex);
        }
    }    
    return NULL;
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
            printf("program being finished...\n");
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

        //  connect to the informed peer
        else if (strncmp(input, "connect ", 8) == 0) {
            int peer_sock = connect_to_peer(input + 8, &p2p_net);
            int *sock_t = malloc(sizeof(int));
            *sock_t = peer_sock;
            pthread_mutex_lock(&p2p_net.net_mutex);
            pthread_create(&p2p_net.peer_threads[p2p_net.threads_count], NULL, handle_peer, sock_t);
            p2p_net.threads_count++;
            pthread_mutex_unlock(&p2p_net.net_mutex);
        }
        
        // mine hash and add message to chat
        else {
            send_chat_message(&p2p_net, input);
        }
    }
}
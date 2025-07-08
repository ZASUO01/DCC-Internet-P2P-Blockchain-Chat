#include "parser.h"
#include "operations.h"
#include "network.h"
#include "p2p-net.h"
#include "logger.h"
#include <stdlib.h>


int main(int argc, char **argv){
    // parse command line arguments
    Params p = parse_args(argc, argv);

    // set debug level
    switch (p.log_mode){
    case 0:
        set_log_level(LOG_DISABLED);
        break;
    case 1:
        set_log_level(LOG_INFO);
        break;
    case 2:
        set_log_level(LOG_DEBUG);
    default:
        break;
    }

    // init p2p net global structure
    init_p2p_net(&p2p_net);

    // init this server
    init_server(&p2p_net);
    LOG_MSG(LOG_INFO, "main() - server initialized");

    pthread_mutex_lock(&p2p_net.net_mutex);
    
    // accept connections thread
    pthread_create(&p2p_net.running_threads[p2p_net.threads_count], NULL, accept_connections, NULL);
    p2p_net.threads_count++;
    LOG_MSG(LOG_INFO, "main() - server ready to accept connections");

    // init the periodic peer requests thread
    pthread_create(&p2p_net.running_threads[p2p_net.threads_count], NULL, periodic_request, NULL);
    p2p_net.threads_count++;
    LOG_MSG(LOG_INFO, "main() - sending periodic requests to peers");

    // connect to known peer if informed
    if(p.addr_str != NULL){
        pthread_mutex_unlock(&p2p_net.net_mutex);
        int peer_fd = connect_to_peer(p.addr_str, &p2p_net);
        pthread_mutex_lock(&p2p_net.net_mutex);
        if(peer_fd < 0){
            LOG_MSG(LOG_ERROR, "main() - failed to connect to known peer");
        }else{
            int *sock_t = malloc(sizeof(int));
            *sock_t = peer_fd;
            pthread_create(&p2p_net.running_threads[p2p_net.threads_count], NULL, handle_peer, sock_t);
            p2p_net.threads_count++;
            LOG_MSG(LOG_INFO, "main() - connected to known peer");
        }
    }
    pthread_mutex_unlock(&p2p_net.net_mutex);

    // read terminal commands
    LOG_MSG(LOG_INFO, "main() - reading user inputs");
    read_inputs();
    
    // wait threads
    for(int i = 0; i < p2p_net.threads_count; i++){
        if(p2p_net.running_threads[i]){
            pthread_join(p2p_net.running_threads[i], NULL);
            p2p_net.running_threads[i] = 0;
        }   
    }
    p2p_net.threads_count = 0;
    
    // cleanup
    clean_p2p_net(&p2p_net);
    LOG_MSG(LOG_INFO, "main() - program finished");
    return 0;
}
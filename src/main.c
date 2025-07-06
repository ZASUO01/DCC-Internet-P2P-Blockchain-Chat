#include "parser.h"
#include "operations.h"
#include "network.h"
#include "defs.h"
#include "p2p-net.h"
#include "logger.h"
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv){
    // parse command line arguments
    Params p = parse_args(argc, argv);

    // set debug level
    if(p.debug_mode > 0){
        set_log_level(LOG_INFO);
    }

    // init p2p net global structure
    init_p2p_net(&p2p_net);

    // init this server
    init_server(&p2p_net);
    LOG_MSG(LOG_INFO, "server initialized");

    // accept connections thread
    pthread_t conn_t;
    pthread_create(&conn_t, NULL, accept_connections, NULL);
    LOG_MSG(LOG_INFO, "server ready to accept connections");

    // connect to the known peer and init a thread for it
    int known_socket = connect_to_peer(p.addr_str, &p2p_net);
    if(known_socket < 0){
        log_exit("connect to known peer failure");
    }
    pthread_t peer_t;
    int *sock = malloc(sizeof(int));
    *sock = known_socket;
    pthread_create(&peer_t, NULL, handle_peer, sock);
    LOG_MSG(LOG_INFO, "connected to known peer %s", p.addr_str);

    // init the periodic peer requests thread
    pthread_t request_t;
    pthread_create(&request_t, NULL, periodic_request, NULL);
    LOG_MSG(LOG_INFO, "periodic request initialized");

    // read terminal commands
    read_inputs();

    // wait threads
    pthread_join(conn_t, NULL);
    pthread_join(peer_t, NULL);
    pthread_join(request_t, NULL);
    for(int i = 0; i < p2p_net.threads_count; i++){
        pthread_join(p2p_net.peer_threads[i], NULL);
    }
    
    // cleanup
    clean_p2p_net(&p2p_net);
    return 0;
}
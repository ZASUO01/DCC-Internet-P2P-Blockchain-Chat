#include "parser.h"
#include "operations.h"
#include "network.h"
#include "defs.h"
#include "p2p-net.h"
#include "logger.h"
#include <unistd.h>

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

    // connect to the known peer and init a thread for it
    int known_socket = connect_to_peer(p.addr_str, &p2p_net);
    pthread_t peer_t;

    int *sock = malloc(sizeof(int));
    *sock = known_socket;
    pthread_create(&peer_t, NULL, handle_peer, sock);
    LOG_MSG(LOG_INFO, "connected to known peer %s", p.addr_str);


    pthread_join(peer_t, NULL);
    
    // cleanup
    close(p2p_net.sock_fd);
    return 0;
}
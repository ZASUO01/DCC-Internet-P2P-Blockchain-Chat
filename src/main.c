#include "parser.h"
#include "network.h"
#include "defs.h"
#include "logger.h"
#include <unistd.h>

int main(int argc, char **argv){
    // parse command line arguments
    Params p = parse_args(argc, argv);

    // set debug level
    if(p.debug_mode > 0){
        set_log_level(LOG_INFO);
    }

    // this server socket
    int sock_fd = set_server_socket("v4");
    LOG_MSG(LOG_INFO, "Server running on PORT: %d", PORT);

    // create accept connections thread

    // connect to known peer


    close(sock_fd);
    return 0;
}
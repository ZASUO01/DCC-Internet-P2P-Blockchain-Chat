#include "parser.h"
#include "logger.h"
#include <string.h>
#include <arpa/inet.h>

static int is_valid_ipv4(const char *addr_str){
  // verify null or empty string
  if (addr_str == NULL || *addr_str == '\0') {
      return 0;
  }

  // inet_pton returns 1 if the managed to convert
  struct sockaddr_in addr;
  return inet_pton(AF_INET, addr_str, &(addr.sin_addr)) == 1;
}

// parse command line arguments
Params parse_args(int argc, char **argv) {
  // set param values
  Params p;
  p.log_mode = 0;
  p.addr_str = NULL;

  // parse argv
  for(int i = 1; i < argc; i++){
      // usage help
      if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0){
        usage(argv[0]);
      }

      // verify info flag
      else if (strcmp(argv[i], "-i") == 0) {
          p.log_mode = 1;
      }
      
      // vefirfy debug flag
      else if(strcmp(argv[i], "-d") == 0){
        p.log_mode = 2;
      }

      // set the ip addr string
      else if(p.addr_str == NULL && is_valid_ipv4(argv[i])){
        p.addr_str = argv[i];
      }

      // invalid parameter
      else{
        usage(argv[0]);
      }
    }
 
  return p;
}
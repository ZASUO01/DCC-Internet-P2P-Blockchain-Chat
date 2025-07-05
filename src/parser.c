#include "parser.h"
#include "logger.h"
#include <string.h>

// parse command line arguments
Params parse_args(int argc, char **argv) {
  // check min arguments number
  if (argc < 2) {
    usage(argv[0]);
  }

  // set param values
  Params p;
  p.addr_str = argv[1];
  p.debug_mode = 0;

  if(argc == 3){
    if(strcmp(argv[2], "-d") == 0){
      p.debug_mode = 1;
    }
  }

  return p;
}
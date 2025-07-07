// file:        parser.h
// description: definitions for the command line arguments parser
#ifndef PARSER_H
#define PARSER_H

// command line arguments
typedef struct {
  int log_mode;
  char *addr_str;
} Params;

// parse the command line arguments
Params parse_args(int argc, char **argv);

#endif
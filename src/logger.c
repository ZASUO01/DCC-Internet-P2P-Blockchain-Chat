#include "logger.h"
#include <stdlib.h>

// logger variables
static LogLevel current_level = LOG_DISABLED;
static FILE *log_file = NULL;

// print program correct usage and exit
void usage(const char *program) {
  fprintf(stderr, "Usage: %s [-h | --help] [-d | -i] [ipv4]\n", program);
  exit(EXIT_SUCCESS);
}

// finish program due failure
void log_exit(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

// convert a log level to a string
static const char *get_level_str(LogLevel level) {
  switch (level) {
  case LOG_INFO:
    return "INFO";
  case LOG_WARNING:
    return "WARNING";
  case LOG_ERROR:
    return "ERROR";
  case LOG_DEBUG:
    return "DEBUG";
  default:
    return "UNKNOWN";
  }
}

// logger setters
void set_log_level(LogLevel level) { current_level = level; }
void set_log_file(FILE *file) { log_file = file; }

// print the log if the current level can be shown
void log_message(LogLevel level, const char *fmt, ...) {
  // only log allowed levels
  if (level < current_level) {
    return;
  }

  // default log file
  if (log_file == NULL) {
    log_file = stderr;
  }

  // print formated string
  va_list args;
  va_start(args, fmt);
  fprintf(log_file, "%s: ", get_level_str(level));
  vfprintf(log_file, fmt, args);
  fprintf(log_file, "\n");
  va_end(args);
}
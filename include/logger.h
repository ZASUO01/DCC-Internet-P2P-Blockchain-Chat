
// file:        logger.h
// description: definitions for useful loggers
#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdio.h>

// simple logger
typedef enum {
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_DISABLED,
} LogLevel;

#define LOG_MSG(level, fmt, ...) log_message(level, fmt, ##__VA_ARGS__)

void log_exit(const char *msg);
void usage(const char *program);
void set_log_level(LogLevel level);
void set_log_file(FILE *file);
char *get_log_file_name(const char *ip);
void log_message(LogLevel level, const char *fmt, ...);

#endif
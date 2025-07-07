
// file:        logger.h
// description: definitions for useful loggers
#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdio.h>

// simple logger
typedef enum {
  LOG_DEBUG,
  LOG_ERROR,
  LOG_WARNING,
  LOG_INFO,
  LOG_DISABLED,
} LogLevel;

#define LOG_MSG(level, fmt, ...) log_message(level, fmt, ##__VA_ARGS__)

void usage(const char *program);
void log_exit(const char *msg);
void set_log_level(LogLevel level);
void set_log_file(FILE *file);
char *get_log_file_name(const char *ip);
void log_message(LogLevel level, const char *fmt, ...);

#endif
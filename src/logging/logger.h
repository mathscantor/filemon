#ifndef LOGGING_LOGGER_H
#define LOGGING_LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdbool.h>
#include <sys/time.h>
#include <stdlib.h>

#include "../helpers/common.h"

typedef enum {
    NIL,
    DEBUG,
    INFO,
    WARNING,
    ERROR
} Severity;

typedef struct {
    char fullpath[PATH_MAX];
    FILE *f;
    char *filetype;
    char *supported_filetypes[4];
} log_file_t;

typedef struct {
    int verbosity_level;
    int verbosity_range[2];
    log_file_t log_file;
} logger_t;

extern const char *severity_colors[];
extern const char *severity_nocolors[];
extern logger_t g_logger;

void logger_init(int, char*);
void log_message(Severity, const char *, const char *, ...);
char *get_current_datetime(void);
char *get_log_extension(char *);
bool is_valid_extension(char *);


#endif

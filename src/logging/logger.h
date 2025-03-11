#ifndef LOGGING_LOGGER_H
#define LOGGING_LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "../helpers/common.h"

#define GREEN_TICK "\x1b[92m\u2714\x1b[0m"
#define RED_CROSS "\x1b[91m\u2718\x1b[0m"

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

void logger_init(int, char*);
void log_message(Severity, int, const char *, const char *, ...);

const char *severity_colors[] = {
    "",                        // NIL
    "\x1b[94m[DBG]\x1b[0m ",   // DEBUG
    "\x1b[92m[INF]\x1b[0m ",   // INFO
    "\x1b[93m[WRN]\x1b[0m ",   // WARNING
    "\x1b[91m[ERR]\x1b[0m "    // ERROR
};

logger_t g_logger = {
    .verbosity_level = 1,
    .verbosity_range = {1, 2},
    .log_file = {
        .fullpath = {0},
        .f = NULL,
        .filetype = NULL,
        .supported_filetypes = {"txt", "csv", "json", "jsonl"}
    }
};

/**
 * @brief 
 * 
 * @param verbosity_level Determines if debug messages will be logged.
 * @param logfile The file path to the log file.
 */
void logger_init(int verbosity_level, char *logfile) {

    g_logger.verbosity_level = verbosity_level;

    if (logfile == NULL) {
        g_logger.log_file.f = NULL;
    } else {
        g_logger.log_file.f = fopen(logfile, "w");
        if (g_logger.log_file.f == NULL) {
            log_message(ERROR, 1, __func__, "Unable to fopen on \"%s\"\n", logfile);
            exit(EXIT_FAILURE);
        }
        if(realpath(logfile, g_logger.log_file.fullpath) == NULL) {
            log_message(ERROR, 1, __func__, "Unable to resolve full path of \"%s\"\n", logfile);
            exit(EXIT_FAILURE);
        }
    }

    // Check if verbosity level is within range
    if (g_logger.verbosity_level < g_logger.verbosity_range[0] || g_logger.verbosity_level > g_logger.verbosity_range[1]) {
        fprintf(stdout, "%s: Verbosity level out of range [%d, %d]\n",
                severity_colors[ERROR], g_logger.verbosity_range[0], g_logger.verbosity_range[1]);
        exit(EXIT_FAILURE);
    }
}

void log_message(Severity sev, int show_time, const char *func, const char *format, ...) {

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* local_time = localtime(&tv.tv_sec);

    int utc_offset = local_time->tm_gmtoff; // tm_gmtoff gives offset in seconds
    int hours_offset = utc_offset / 3600;
    int minutes_offset = abs((utc_offset % 3600) / 60);

    // If verbosity is default (1), then ignore DEBUG messages
    if (g_logger.verbosity_level == 1 && sev == DEBUG) {
        return;
    }

    va_list args;
    if (g_logger.log_file.f != NULL){
        if (show_time) {
            fprintf(g_logger.log_file.f, 
                "[%02d-%02d-%04d %02d:%02d:%02d.%03d",
                local_time->tm_mday,
                local_time->tm_mon + 1,
                local_time->tm_year + 1900,
                local_time->tm_hour,
                local_time->tm_min,
                local_time->tm_sec,
                (int)tv.tv_usec / 1000);
            if (hours_offset >= 0)
                fprintf(g_logger.log_file.f, " UTC+%02d:%02d] ", hours_offset, minutes_offset);
            else {
                fprintf(g_logger.log_file.f, " UTC-%02d:%02d] ", abs(hours_offset), minutes_offset);
            }
        }
        fprintf(g_logger.log_file.f, "%s", severity_colors[sev]);
        fprintf(g_logger.log_file.f, "%s:: ", func);
        va_start(args, format);
        vfprintf(g_logger.log_file.f, format, args);
        fflush(g_logger.log_file.f);
        va_end(args);
    } else {
        if (show_time) {
            printf("[%02d-%02d-%04d %02d:%02d:%02d.%03d",
                local_time->tm_mday,
                local_time->tm_mon + 1,
                local_time->tm_year + 1900,
                local_time->tm_hour,
                local_time->tm_min,
                local_time->tm_sec,
                (int)tv.tv_usec / 1000);
            if (hours_offset >= 0)
                printf(" UTC+%02d:%02d] ", hours_offset, minutes_offset);
            else {
                printf(" UTC-%02d:%02d] ", abs(hours_offset), minutes_offset);
            }
        }
        printf("%s", severity_colors[sev]);
        printf("%s:: ", func);
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

#endif

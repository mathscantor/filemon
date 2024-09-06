#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#define GREEN_TICK "\x1b[92m\u2714\x1b[0m"
#define RED_CROSS "\x1b[91m\u2718\x1b[0m"

typedef enum {
    NIL,
    DEBUG,
    INFO,
    WARNING,
    ERROR
} Severity;

typedef struct Logger{
    int verbosity_level;
    int verbosity_range[2];
    char logfile[PATH_MAX];
    FILE* f_logfile;
} logger_t;

void logger_init(int verbosity_level, char* logfile);
void log_message(Severity sev, int show_time, const char *format, ...);

const char *severity_colors[] = {
    "",                        // NIL
    "\x1b[94m[DBG]\x1b[0m ",   // DEBUG
    "\x1b[92m[INF]\x1b[0m ",   // INFO
    "\x1b[93m[WRN]\x1b[0m ",   // WARNING
    "\x1b[91m[ERR]\x1b[0m "    // ERROR
};

logger_t g_logger;

/**
 * @brief 
 * 
 * @param verbosity_level Determines if debug messages will be logged.
 * @param logfile The file path to the log file.
 */
void logger_init(int verbosity_level, char* logfile) {
    g_logger.verbosity_level = verbosity_level;
    g_logger.verbosity_range[0] = 1;
    g_logger.verbosity_range[1] = 2;
    if (logfile == NULL) {
        memset(g_logger.logfile, 0, sizeof(g_logger.logfile));
        g_logger.f_logfile = NULL;
    } else {
        strncpy(g_logger.logfile, logfile, strlen(logfile));
        g_logger.f_logfile = fopen(g_logger.logfile, "w");
    }

    // Check if verbosity level is within range
    if (g_logger.verbosity_level < g_logger.verbosity_range[0] || g_logger.verbosity_level > g_logger.verbosity_range[1]) {
        fprintf(stdout, "%s: Verbosity level out of range [%d, %d]\n",
                severity_colors[ERROR], g_logger.verbosity_range[0], g_logger.verbosity_range[1]);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Log a message with a given severity level.
 * 
 * @param sev The severity level of the log message.
 * @param show_time 1 to show time, 0 to not display time.
 * @param format The format string.
 * @param ... 
 */
void log_message(Severity sev, int show_time, const char *format, ...) {

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* local_time = localtime(&tv.tv_sec);

    // If verbosity is default (1), then ignore DEBUG messages
    if (g_logger.verbosity_level == 1 && sev == DEBUG) {
        return;
    }

    va_list args;
    if (g_logger.f_logfile != NULL){
        if (show_time) {
            fprintf(g_logger.f_logfile, 
                "%02d-%02d-%04d %02d:%02d:%02d.%03d UTC%s:00%-4s",
                local_time->tm_mday,
                local_time->tm_mon + 1,
                local_time->tm_year + 1900,
                local_time->tm_hour,
                local_time->tm_min,
                local_time->tm_sec,
                (int)tv.tv_usec / 1000,
                local_time->tm_zone,
                "");
        }
        va_start(args, format);
        fprintf(g_logger.f_logfile, "%s", severity_colors[sev]);
        vfprintf(g_logger.f_logfile, format, args);
        fflush(g_logger.f_logfile);
        va_end(args);
    } else {
        if (show_time) {
            printf("%02d-%02d-%04d %02d:%02d:%02d.%03d UTC%s:00%4s",
                local_time->tm_mday,
                local_time->tm_mon + 1,
                local_time->tm_year + 1900,
                local_time->tm_hour,
                local_time->tm_min,
                local_time->tm_sec,
                (int)tv.tv_usec / 1000,
                local_time->tm_zone,
                "");
        }
        va_start(args, format);
        printf("%s", severity_colors[sev]);
        vprintf(format, args);
        va_end(args);
    }
}

#endif

#ifndef LOGGING_LOGGER_H
#define LOGGING_LOGGER_H

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
void log_message(Severity, const char *, const char *, ...);
char *get_current_datetime(void);
char *get_log_extension(char *);
bool is_valid_extension(char *);

const char *severity_colors[] = {
    "",                       // NIL
    "\x1b[94m[DBG]\x1b[0m",   // DEBUG
    "\x1b[92m[INF]\x1b[0m",   // INFO
    "\x1b[93m[WRN]\x1b[0m",   // WARNING
    "\x1b[91m[ERR]\x1b[0m"    // ERROR
};

const char *severity_nocolors[] = {
    "",        // NIL
    "[DBG]",   // DEBUG
    "[INF]",   // INFO
    "[WRN]",   // WARNING
    "[ERR]"    // ERROR
};

logger_t g_logger = {
    .verbosity_level = 1,
    .verbosity_range = {1, 2},
    .log_file = {
        .fullpath = {0},
        .f = NULL,
        .filetype = NULL,
        .supported_filetypes = {".txt", ".csv", ".json", ".jsonl"}
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
        g_logger.log_file.filetype = get_log_extension(logfile);
        if (g_logger.log_file.filetype == NULL) {
            log_message(ERROR, __func__, "No file extension detected! Valid extensions: [\".txt\", \".csv\", \".json\", \".jsonl\"]");
            exit(EXIT_FAILURE);
        }
        if (!is_valid_extension(g_logger.log_file.filetype)) {
            log_message(ERROR, __func__, "Invalid \"%s\" extension type! Valid extensions: [\".txt\", \".csv\", \".json\", \".jsonl\"]", g_logger.log_file.filetype);
            exit(EXIT_FAILURE);
        }
        g_logger.log_file.f = fopen(logfile, "w");
        if (g_logger.log_file.f == NULL) {
            log_message(ERROR, __func__, "Unable to fopen on \"%s\"", logfile);
            exit(EXIT_FAILURE);
        }
        if(realpath(logfile, g_logger.log_file.fullpath) == NULL) {
            log_message(ERROR, __func__, "Unable to resolve full path of \"%s\"", logfile);
            exit(EXIT_FAILURE);
        }

        // Write CSV headers
        if (strcmp(g_logger.log_file.filetype, ".csv") == 0) {
            fprintf(g_logger.log_file.f, "datetime,severity,function,message\n");
        }
    }

    // Check if verbosity level is within range
    if (g_logger.verbosity_level < g_logger.verbosity_range[0] || g_logger.verbosity_level > g_logger.verbosity_range[1]) {
        fprintf(stdout, "%s: Verbosity level out of range [%d, %d]\n",
                severity_colors[ERROR], g_logger.verbosity_range[0], g_logger.verbosity_range[1]);
        exit(EXIT_FAILURE);
    }
}

void log_message(Severity sev, const char *func, const char *format, ...) {

    va_list args;
    char *current_datetime = get_current_datetime();
    // If verbosity is default (1), then ignore DEBUG messages
    if (g_logger.verbosity_level == 1 && sev == DEBUG) {
        return;
    }

    if (!g_logger.log_file.f) 
        goto stdout_format;

    if (strcmp(g_logger.log_file.filetype, ".txt") == 0)
        goto txt_format;
    else if (strcmp(g_logger.log_file.filetype, ".csv") == 0)
        goto csv_format;
    else if (strcmp(g_logger.log_file.filetype, ".json") == 0)
        goto json_format;
    else if (strcmp(g_logger.log_file.filetype, ".jsonl") == 0)
        goto jsonl_format;


stdout_format:
    printf("[%s] ", current_datetime);
    printf("%s ", severity_colors[sev]);
    printf("%s - ", func);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    free(current_datetime);
    current_datetime = NULL;
    return;

txt_format:
    fprintf(g_logger.log_file.f, "[%s] ", current_datetime);
    fprintf(g_logger.log_file.f, "%s ", severity_nocolors[sev]);
    fprintf(g_logger.log_file.f, "%s - ", func);

    va_start(args, format);
    vfprintf(g_logger.log_file.f, format, args);
    va_end(args);

    fprintf(g_logger.log_file.f, "\n");
    fflush(g_logger.log_file.f);
    free(current_datetime);
    current_datetime = NULL;
    return;

csv_format:
    fprintf(g_logger.log_file.f, "\"%s\",", current_datetime);
    fprintf(g_logger.log_file.f, "\"%s\",", severity_nocolors[sev]);
    fprintf(g_logger.log_file.f, "\"%s\",", func);

    fprintf(g_logger.log_file.f, "\"");
    va_start(args, format);
    vfprintf(g_logger.log_file.f, format, args);
    va_end(args);
    fprintf(g_logger.log_file.f, "\"");

    fprintf(g_logger.log_file.f, "\n");
    fflush(g_logger.log_file.f);
    free(current_datetime);
    current_datetime = NULL;
    return;

json_format:
    fprintf(g_logger.log_file.f, "{\n");
    fprintf(g_logger.log_file.f, "  \"datetime\": \"%s\",\n", current_datetime);
    fprintf(g_logger.log_file.f, "  \"severity\": \"%s\",\n", severity_nocolors[sev]);
    fprintf(g_logger.log_file.f, "  \"function\": \"%s\",\n", func);
    fprintf(g_logger.log_file.f, "  \"message\": \"");

    va_start(args, format);
    vfprintf(g_logger.log_file.f, format, args);
    va_end(args);

    fprintf(g_logger.log_file.f, "\"\n"); // Closing the message value
    fprintf(g_logger.log_file.f, "}\n");  // Closing the JSON object
    fflush(g_logger.log_file.f);
    free(current_datetime);
    current_datetime = NULL;
    return;

jsonl_format:
    fprintf(g_logger.log_file.f, "{");
    fprintf(g_logger.log_file.f, "\"datetime\": \"%s\",", current_datetime);
    fprintf(g_logger.log_file.f, "\"severity\": \"%s\",", severity_nocolors[sev]);
    fprintf(g_logger.log_file.f, "\"function\": \"%s\",", func);
    fprintf(g_logger.log_file.f, "\"message\": \"");

    va_start(args, format);
    vfprintf(g_logger.log_file.f, format, args);
    va_end(args);

    fprintf(g_logger.log_file.f, "\"}\n");
    fflush(g_logger.log_file.f);
    free(current_datetime);
    current_datetime = NULL;
    return;

}

char *get_current_datetime(void) {

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* local_time = localtime(&tv.tv_sec);

    int utc_offset = local_time->tm_gmtoff;
    int hours_offset = utc_offset / 3600;
    int minutes_offset = abs((utc_offset % 3600) / 60);

    // Eg. 11-03-2025 14:12:41.195 UTC+08:00
    char *current_datetime = (char *)malloc(100);
    if (hours_offset >= 0) {
        snprintf(current_datetime, 100, "%02d-%02d-%04d %02d:%02d:%02d.%03d UTC+%02d:%02d", 
            local_time->tm_mday,
            local_time->tm_mon + 1,
            local_time->tm_year + 1900,
            local_time->tm_hour,
            local_time->tm_min,
            local_time->tm_sec,
            (int)tv.tv_usec / 1000,
            hours_offset,
            minutes_offset);
    } else {
        snprintf(current_datetime, 100, "%02d-%02d-%04d %02d:%02d:%02d.%03d UTC-%02d:%02d", 
            local_time->tm_mday,
            local_time->tm_mon + 1,
            local_time->tm_year + 1900,
            local_time->tm_hour,
            local_time->tm_min,
            local_time->tm_sec,
            (int)tv.tv_usec / 1000,
            abs(hours_offset),
            minutes_offset);
    }

    return current_datetime;
}

bool is_valid_extension(char *ext) {

    size_t supported_extension_size = sizeof(g_logger.log_file.supported_filetypes) / sizeof (char *);

    for (size_t i = 0; i < supported_extension_size; i++) {
        if (strcmp(ext, g_logger.log_file.supported_filetypes[i]) == 0) {
            return true;
        }
    }
    return false;
}

char *get_log_extension(char *path) {

    char *ext = strrchr(path, '.'); 
    if (!ext || ext == path) return NULL;  
    
    return ext;
}
#endif

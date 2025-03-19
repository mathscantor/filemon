#include "logger.h"

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
 * @brief Initializes the logger with the specified verbosity level and log file.
 * 
 * This function initializes the global logger structure based on the provided verbosity level and 
 * optionally opens a log file. If a log file is provided, it validates the file extension and 
 * prepares it for logging. If the verbosity level is out of range, the program will exit with an error.
 * The log file can be a text file (.txt), CSV (.csv), JSON (.json), or JSONL (.jsonl). 
 * In case of a CSV file, the function writes the header row in the format: 
 * `datetime,severity,function,message`.
 * 
 * @param verbosity_level The verbosity level to be used by the logger. It determines the level of detail for the logs.
 *                        The valid range is defined by `g_logger.verbosity_range`.
 * @param logfile The path to the log file. If `NULL`, logging will not be directed to a file.
 *                If specified, the file extension must be one of the valid types: [".txt", ".csv", ".json", ".jsonl"].
 * 
 * @note If the log file extension is not valid or cannot be opened, the program will exit with an error.
 * @note If `verbosity_level` is outside the valid range, the program will exit with an error.
 * 
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

/**
 * @brief Logs a message with a specific severity to a log file or stdout.
 * 
 * This function logs a message with the specified severity level and additional
 * information to a file or stdout, depending on the logging configuration.
 * It supports multiple log file formats, including plain text, CSV, JSON, and JSONL.
 * The message is printed with the current datetime, severity level, function name, 
 * and the message content.
 * 
 * @param sev The severity level of the log (DEBUG, INFO, WARNING, ERROR).
 * @param func The name of the function where the log is being generated.
 * @param format The format string for the log message, followed by any arguments.
 * @param ... The arguments to be formatted and included in the log message.
 */
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
    SAFE_FREE(current_datetime);
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
    SAFE_FREE(current_datetime);
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
    SAFE_FREE(current_datetime);
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
    SAFE_FREE(current_datetime);
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
    SAFE_FREE(current_datetime);
    return;

}

/**
 * @brief Get the current datetime as a formatted string.
 * 
 * This function retrieves the current datetime in the format 
 * "DD-MM-YYYY HH:MM:SS.mmm UTC±hh:mm", where the date and time are based on 
 * the local system time, and the UTC offset is included.
 * 
 * @return A string representing the current datetime with the UTC offset.
 */
char *get_current_datetime(void) {

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *local_time = localtime(&tv.tv_sec);
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

/**
 * @brief Checks if a file extension is valid.
 * 
 * This function checks if the given file extension is supported by the logger.
 * It compares the provided extension with a list of supported extensions.
 * 
 * @param ext The file extension to be validated.
 * 
 * @return `true` if the extension is valid, `false` otherwise.
 */
bool is_valid_extension(char *ext) {

    size_t supported_extension_size = sizeof(g_logger.log_file.supported_filetypes) / sizeof (char *);

    for (size_t i = 0; i < supported_extension_size; i++) {
        if (strcmp(ext, g_logger.log_file.supported_filetypes[i]) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Extracts the file extension from a file path.
 * 
 * This function extracts the file extension from the given file path.
 * If no extension is found, it returns `NULL`.
 * 
 * @param path The file path from which to extract the extension.
 * 
 * @return The file extension, or `NULL` if no extension is found.
 */
char *get_log_extension(char *path) {

    char *ext = strrchr(path, '.'); 
    if (!ext || ext == path) return NULL;  
    
    return ext;
}
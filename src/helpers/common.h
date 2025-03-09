#ifndef HELPERS_COMMON_H
#define HELPERS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <stdint.h>
#include <fstab.h>
#include <sys/stat.h> 
#include <sys/utsname.h>

#include "../logging/logger.h"

#define SAFE_FREE(ptr) do { \
    if ((ptr) != NULL) {    \
        free(ptr);          \
        (ptr) = NULL;       \
    }                       \
} while (0)

#define MAX_PROCESS_FILTER 64
#define MAX_REGEX_LEN 1024
#define MAX_PROCESS_NAME_LEN 16

char *get_path_from_fd(int fd);
char *get_comm_from_pid(int pid);
bool path_exists(const char* path);
bool is_directory(const char* path);
bool regex_search(regex_t *, const char* );
char *get_full_path(const char *);
bool is_valid_integer(const char *);
char *uint32_array_to_string(const uint32_t *, size_t) ;
bool is_in_uint32_array(uint32_t *haystack, size_t size, uint32_t needle);
char *concatenate_process_names(char [][MAX_PROCESS_NAME_LEN], size_t);
bool is_in_process_names(char [][MAX_PROCESS_NAME_LEN], size_t, char *);


/**
 * @brief Get the path from fd object
 * 
 * @param fd The fanotify fd.
 * @return char* The file path or directory path.
 */
char *get_path_from_fd(int fd) {
    ssize_t len;
    char* filepath = (char*)malloc(PATH_MAX);
    char* fd_path = (char*)malloc(PATH_MAX);
    if (fd <= 0) {
        return NULL;
    }
    snprintf(fd_path, PATH_MAX, "/proc/self/fd/%d", fd);
    if ((len = readlink(fd_path, filepath, PATH_MAX - 1)) < 0) {
        return NULL;
    }
    filepath[len] = '\0';
    return filepath;
}

/**
 * @brief Get the comm from pid object
 * 
 * @param pid The PID of the process that triggered the fan event.
 * @return char* The process name.
 */
char* get_comm_from_pid(int pid){
    char* comm_path = (char*)malloc(PATH_MAX);
    char* comm = (char*)malloc(16);
    snprintf(comm_path, PATH_MAX, "/proc/%d/comm", pid);
    FILE *comm_file = fopen(comm_path, "r");

    if (comm_file == NULL) {
        // unknown because the short-lived process already finished before we can even get the name
        return "unknown-process";
    }

    if (fgets(comm, 16, comm_file)) {
        comm[strcspn(comm, "\n")] = '\0';
    }
    close(fileno(comm_file));
    if (comm[0] == '\0' || comm[0] == ' ') {
        return "unknown-process";
    }
    return comm;
}

bool path_exists(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) == 0) 
        return true;
    return false;
}

bool is_directory(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
        return true;
    }
    return false;
}

bool regex_search(regex_t *expr, const char* haystack) {

    int ret;
    ret = regexec(expr, haystack, 0, NULL, 0);
    if (!ret) {
        return true;
    }
    return false;
}

char *get_full_path(const char *path) {
    char *resolved_path = malloc(PATH_MAX);
    if (resolved_path == NULL) {
        log_message(ERROR, 1, __func__, "Unable to malloc for resolved_path\n");
        return NULL;
    }

    if (realpath(path, resolved_path) == NULL) {
        log_message(ERROR, 1, __func__, "Unable to resolve fullpath of: %s\n", path);
        free(resolved_path);
        return NULL;
    }

    return resolved_path;
}

bool is_valid_integer(const char *str) {
    char *endptr;
    errno = 0;  // To distinguish success/failure after call

    // Convert string to long
    long val = strtol(str, &endptr, 10);

    // Check for various possible errors
    if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) {
        return false;  // Out of range
    }

    if (errno != 0 && val == 0) {
        return false;  // General error
    }

    if (endptr == str) {
        return false;  // No digits were found
    }

    // If there are extra characters after the number, it's not valid
    if (*endptr != '\0') {
        return false;
    }

    return true;
}

char *uint32_array_to_string(const uint32_t *arr, size_t arr_len) {
    char *result = NULL;
    char tmp[13] = {0};
    size_t buf_length = 0;

    if (!arr) return strdup("");

    if (arr[0] == 0 || arr_len == 0) return strdup("");

    for (size_t i = 0; i < arr_len; i++) {
        if (arr[i] == 0) break;
        sprintf(tmp, "%u, ", arr[i]);
        buf_length += strlen(tmp);
        memset(tmp, 0, sizeof(tmp));
    }
    
    result = (char *)malloc(buf_length);
    char *ptr = result;
    if (!result) return NULL;

    for (size_t i = 0; i < arr_len; i++) {
        if (arr[i] == 0) break;
        ptr += sprintf(ptr, "%u%s", arr[i], (i < arr_len - 1 && arr[i + 1] != 0) ? ", " : "");
    }

    return result;
}

bool is_in_uint32_array(uint32_t *haystack, size_t size, uint32_t needle) {
    for (size_t i = 0; i < size; i++) {
        if (needle == haystack[i]) {
            return true;
        }
    }
    return false;
}

char *concatenate_process_names(char arr[][MAX_PROCESS_NAME_LEN], size_t arr_len) {
    
    char *result = NULL;
    size_t buf_length = 0;

    if (!arr[0]) return strdup("");

    if (arr[0][0] == '\0' || arr_len == 0) return strdup("");

    for(size_t i = 0; i < arr_len; i++) {
        if (arr[i][0] == '\0') break;
        buf_length += strlen(arr[i]) + 3;
    }

    result = (char *)malloc(buf_length);
    if (!result) return NULL; 

    char *ptr = result; 
    for (size_t i = 0; i < arr_len; i++) {
        if (arr[i][0] == '\0') break;
        ptr += sprintf(ptr, "\"%s\"%s", arr[i], (i < arr_len - 1 && arr[i + 1][0] != '\0') ? ", " : "");
    }

    return result;
}

bool is_in_process_names(char haystack[][MAX_PROCESS_NAME_LEN], size_t size, char *needle) {
    for (size_t i = 0; i < size; i++) {
        if (strncmp(haystack[i], needle, strlen(needle)) == 0) {
            return true;
        }
    }
    return false;
}

#endif

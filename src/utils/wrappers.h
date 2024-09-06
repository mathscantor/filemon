#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <fstab.h>
#include <sys/stat.h> 
#include <sys/utsname.h>
#include "logger.h"

#ifndef WRAPPER_H
#define WRAPPER_H

#define FILTER_MAX 1024
#define FLAGS_MAX 1024
#define PROC_NAME_LEN 16

char* get_path_from_fd(int fd);
char* get_comm_from_pid(int pid);
int path_exists(const char* path);
int is_directory(const char* path);
int regex_search(regex_t expr, const char* haystack );
int has_config_fanotify();
int has_config_fanotify_access_perms();
char* get_full_path(const char *path);
int is_valid_integer(const char *str);
char* strcat_int_array(int *array, size_t size);
int is_in_int_array(int *haystack, size_t size, int needle);
char* strcat_process_names(char array[][PROC_NAME_LEN], size_t size);
int is_in_process_names(char haystack[][PROC_NAME_LEN], size_t size, char* needle);

/**
 * @brief Get the path from fd object
 * 
 * @param fd The fanotify fd.
 * @return char* The file path or directory path.
 */
char* get_path_from_fd(int fd) {
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

/**
 * @brief Checks if a file/directory path exists.
 * 
 * @param path The file/directory path.
 * @return int 
 */
int path_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

/**
 * @brief Checks if a particular path is a directory.
 * 
 * @param path The file/directory path.
 * @return int 
 */
int is_directory(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
        return 1;
    }
    return 0;
}

/**
 * @brief Searches for the regular expression in the haystack. Returns 1 on success, otherwise 0.
 * 
 * @param expr The compiled regular expression.
 * @param haystack A string blob.
 * @return int 
 */
int regex_search(regex_t expr, const char* haystack) {

    int ret;
    ret = regexec(&expr, haystack, 0, NULL, 0);
    if (!ret) {
        return 1;
    }
    return 0;
}

/**
 * @brief Checks if CONFIG_FANOTIFY is enabled. Returns 1 on success, otherwise 0.
 * 
 * @return int 
 */
int has_config_fanotify() {
    struct utsname uname_data;
    FILE *file;
    char filepath[256];
    char line[256];
    const char *search_str = "CONFIG_FANOTIFY=y";

    // Get the kernel version
    if (uname(&uname_data) != 0) {
        log_message(ERROR, 1, "Unable to get kernel version via uname.\n");
        return 0;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        log_message(ERROR, 1, "Unable to open: \"/boot/config-%s\"\n", uname_data.release);
        return 0;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

/**
 * @brief Checks if CONFIG_FANOTIFY_ACCESS_PERMISSIONS is enabled. Returns 1 on sucess, otherwise 0.
 * 
 * @return int 
 */
int has_config_fanotify_access_perms() {
    struct utsname uname_data;
    FILE *file;
    char filepath[256];
    char line[256];
    const char *search_str = "CONFIG_FANOTIFY_ACCESS_PERMISSIONS=y";

    // Get the kernel version
    if (uname(&uname_data) != 0) {
        log_message(ERROR, 1, "Unable to get kernel version via uname.\n");
        return 0;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        log_message(ERROR, 1, "Unable to fopen: %s\n", filepath);
        return 0;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

/**
 * @brief Get the full path given any path.
 * 
 * @param path 
 * @return char* 
 */
char* get_full_path(const char *path) {
    char *resolved_path = malloc(PATH_MAX);
    if (resolved_path == NULL) {
        log_message(ERROR, 1, "Unable to malloc for resolved_path\n");
        return NULL;
    }

    if (realpath(path, resolved_path) == NULL) {
        log_message(ERROR, 1, "Unable to resolve fullpath of: %s\n", path);
        free(resolved_path);
        return NULL;
    }

    return resolved_path;
}

/**
 * @brief Get the mount information of a given path.
 * 
 * @param path 
 * @return struct fstab* 
 */
struct fstab *getfssearch(const char *path) {
    /* stat the file in question */
    struct stat path_stat;
    stat(path, &path_stat);

    /* iterate through the list of devices */
    struct fstab *fs = NULL;
    while( (fs = getfsent()) ) {
        /* stat the mount point */
        struct stat dev_stat;
        stat(fs->fs_file, &dev_stat);

        /* check if our file and the mount point are on the same device */
        if( dev_stat.st_dev == path_stat.st_dev ) {
            break;
        }
    }
    endfsent();
    return fs;
}

/**
 * @brief Pre-check to be used before converting a string into an integer.
 * 
 * @param str A string.
 * @return int 
 */
int is_valid_integer(const char *str) {
    char *endptr;
    errno = 0;  // To distinguish success/failure after call

    // Convert string to long
    long val = strtol(str, &endptr, 10);

    // Check for various possible errors
    if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) {
        return 0;  // Out of range
    }

    if (errno != 0 && val == 0) {
        return 0;  // General error
    }

    if (endptr == str) {
        return 0;  // No digits were found
    }

    // If there are extra characters after the number, it's not valid
    if (*endptr != '\0') {
        return 0;
    }

    return 1;
}

/**
 * @brief Returns a concatenated string from an array of integers.
 * 
 * @param array The integer array.
 * @param size  The maximum length of the integer array.
 * @return char* 
 */
char* strcat_int_array(int *array, size_t size){
    
    char* string = malloc(FILTER_MAX + 1);
    memset(string, 0, FILTER_MAX + 1);
    int length = 0;

    for(size_t i = 0; i < size; i++) { 
        if (array[i] == 0)
            break;

        length = snprintf(NULL, 0, "%d, ", array[i]);
        char* tmp = malloc(length + 1);
        snprintf(tmp, length + 1, "%d, ", array[i]);
        strncat(string, tmp, strlen(tmp) + 1);
    }
    string[strlen(string) - 2] = '\0';
    return string;
}

/**
 * @brief Check for existence of integer in an array of integers.
 * @param haystack The integer array.
 * @param size The maximum length of the integer array.
 * @param needle The integer to look for within the integer array.
 * @return int 
 */
int is_in_int_array(int *haystack, size_t size, int needle) {
    for (size_t i = 0; i < size; i++) {
        if (needle == haystack[i]) {
            return 1;
        }
    }
    return 0;
}

char* strcat_process_names(char array[][PROC_NAME_LEN], size_t size){
    
    char* string = malloc(FILTER_MAX + 1);
    memset(string, 0, FILTER_MAX + 1);
    int length = 0;

    for(size_t i = 0; i < size; i++) { 
        if (array[i][0] == '\0')
            break;

        length = snprintf(NULL, 0, "%s, ", array[i]);
        char* tmp = malloc(length + 1);
        snprintf(tmp, length + 1, "%s, ", array[i]);
        strncat(string, tmp, strlen(tmp) + 1);
    }
    string[strlen(string) - 2] = '\0';
    return string;
}

int is_in_process_names(char haystack[][PROC_NAME_LEN], size_t size, char* needle) {
    for (size_t i = 0; i < size; i++) {
        if (strncmp(haystack[i], needle, strlen(needle)) == 0) {
            return 1;
        }
    }
    return 0;
}
#endif

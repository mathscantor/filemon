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

char* get_path_from_fd(int fd);
char* get_comm_from_pid(int pid);
int path_exists(const char* path);
int is_directory(const char* path);
int regex_search(regex_t expr, const char* haystack );
int has_config_fanotify();
int has_config_fanotify_access_perms();
char* get_full_path(const char *path);

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
    char* comm = (char*)malloc(PATH_MAX);
    snprintf(comm_path, PATH_MAX, "/proc/%d/comm", pid);
    FILE *comm_file = fopen(comm_path, "r");

    if (comm_file == NULL) {
        // unknown because the short-lived process already finished before we can even get the name
        return "unknown-process";
    }

    if (fgets(comm, sizeof(comm), comm_file)) {
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
        perror("uname");
        log_message(ERROR, 1, "Unable to get kernel version.");
        return 0;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        log_message(ERROR, 1, "Unable to open: \"/boot/config-%s\"", uname_data.release);
        return 0;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, search_str)) {
            #ifndef CONFIG_FANOTIFY_ENABLED
            #define CONFIG_FANOTIFY_ENABLED
            #endif
            log_message(DEBUG, 1, "CONFIG_FANOTIFY Enabled: " GREEN_TICK "\n");
            fclose(file);
            return 1;
        }
    }
    log_message(DEBUG, 1, "CONFIG_FANOTIFY Enabled: " RED_CROSS "\n");
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
        perror("uname");
        return 0;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        perror("fopen");
        return 0;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, search_str)) {
            #ifndef CONFIG_FANOTIFY_ACCESS_PERMISSIONS_ENABLED
            #define CONFIG_FANOTIFY_ACCESS_PERMISSIONS_ENABLED
            #endif
            log_message(DEBUG, 1, "CONFIG_FANOTIFY_ACCESS_PERMISSIONS Enabled: " GREEN_TICK "\n");
            fclose(file);
            return 1;
        }
    }
    log_message(DEBUG, 1, "CONFIG_FANOTIFY_ACCESS_PERMISSIONS Enabled: " RED_CROSS "\n");
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
        perror("malloc");
        return NULL;
    }

    if (realpath(path, resolved_path) == NULL) {
        perror("realpath");
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

#endif

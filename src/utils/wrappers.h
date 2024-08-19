#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <sys/utsname.h>

#ifndef WRAPPER_H
#define WRAPPER_H

char* get_path_from_fd(int fd);
char* get_comm_from_pid(int pid);
int path_exists(const char* path);
int is_directory(const char* path);
int regex_search(regex_t expr, const char* haystack );
int has_config_fanotify();
int has_config_fanotify_access_perms();

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
        return "unknown";
    }

    if (fgets(comm, sizeof(comm), comm_file)) {
        comm[strcspn(comm, "\n")] = '\0';
    }
    close(fileno(comm_file));
    if (comm[0] == '\0' || comm[0] == ' ') {
        return "unknown";
    }
    return comm;
}

int path_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

int is_directory(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
        return 1;
    }
    return 0;
}

int regex_search(regex_t expr, const char* haystack) {

    int ret;
    ret = regexec(&expr, haystack, 0, NULL, 0);
    if (!ret) {
        return 1;
    }
    return 0;
}

int has_config_fanotify() {
    struct utsname uname_data;
    FILE *file;
    char filepath[256];
    char line[256];
    const char *search_str = "CONFIG_FANOTIFY=y";

    // Get the kernel version
    if (uname(&uname_data) != 0) {
        perror("uname");
        return 1;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, search_str)) {
            #ifndef CONFIG_FANOTIFY_ENABLED
            #define CONFIG_FANOTIFY_ENABLED
            #endif
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

int has_config_fanotify_access_perms() {
    struct utsname uname_data;
    FILE *file;
    char filepath[256];
    char line[256];
    const char *search_str = "CONFIG_FANOTIFY_ACCESS_PERMISSIONS=y";

    // Get the kernel version
    if (uname(&uname_data) != 0) {
        perror("uname");
        return 1;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, search_str)) {
            #ifndef CONFIG_FANOTIFY_ACCESS_PERMISSIONS_ENABLED
            #define CONFIG_FANOTIFY_ACCESS_PERMISSIONS_ENABLED
            #endif
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}
#endif

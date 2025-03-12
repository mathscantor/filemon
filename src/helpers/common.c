#include "common.h"

/**
 * @brief Get the path from fd object
 * 
 * @param fd The fanotify fd.
 * @return char* The file path or directory path.
 */
char *get_path_from_fd(int fd) {
    ssize_t len;
    char *filepath = (char *)malloc(PATH_MAX);
    char *fd_path = (char *)malloc(PATH_MAX);
    if (fd <= 0) {
        return NULL;
    }
    snprintf(fd_path, PATH_MAX, "/proc/self/fd/%d", fd);
    if ((len = readlink(fd_path, filepath, PATH_MAX - 1)) < 0) {
        return NULL;
    }
    filepath[len] = '\0';
    SAFE_FREE(fd_path);
    return filepath;
}

/**
 * @brief Get the comm from pid object
 * 
 * @param pid The PID of the process that triggered the fan event.
 * @return char* The process name.
 */
char *get_comm_from_pid(uint32_t pid){

    char *comm_path = (char*)malloc(PATH_MAX);
    if (comm_path == NULL) {
        // log_message(WARNING, __func__, "Unable to allocate %d bytes to comm_path!", MAX_PROCESS_NAME_LEN);
        return NULL;
    }

    snprintf(comm_path, PATH_MAX, "/proc/%d/comm", pid);
    FILE *comm_file = fopen(comm_path, "r");
    if (comm_file == NULL) {
        // unknown because the short-lived process already finished before we can even get the name
        // log_message(WARNING, __func__, "Unable to fopen \"%s\"!", comm_path);
        SAFE_FREE(comm_path);
        return NULL;
    }

    char *comm = (char*)malloc(MAX_PROCESS_NAME_LEN);
    if (comm == NULL) {
        // log_message(WARNING, __func__, "Unable to allocate %d bytes to comm!", MAX_PROCESS_NAME_LEN);
        SAFE_FREE(comm_path);
        return NULL;
    }

    if (fgets(comm, 16, comm_file)) {
        comm[strcspn(comm, "\n")] = '\0';
    }
    close(fileno(comm_file));
    if (comm[0] == '\0' || comm[0] == ' ') {
        SAFE_FREE(comm_path);
        return NULL;
    }

    for (size_t i = 0; i < MAX_PROCESS_NAME_LEN; i++){
        if (comm[i] != '\0' && !is_printable_ascii(comm[i])) {
            SAFE_FREE(comm_path);
            return NULL;
        }
    }

    SAFE_FREE(comm_path);
    return comm;
}

char *get_comm_from_cache(uint32_t pid, comm_cache_t *comm_cache) {
   
    if (comm_cache->comms[pid % MAX_COMM_CACHE_SIZE][0] == '\0')
        return NULL;

    return strdup(comm_cache->comms[pid % MAX_COMM_CACHE_SIZE]);
}


void set_comm_to_cache(uint32_t pid, char *comm, comm_cache_t *comm_cache) {

    if (strcmp(comm, comm_cache->comms[pid % MAX_COMM_CACHE_SIZE]) == 0) {
        // log_message(DEBUG, __func__, "Cache index %d already contains \"%s\". Ignoring...", pid % MAX_COMM_CACHE_SIZE, comm);
        return;
    }
        
    memset(comm_cache->comms[pid % MAX_COMM_CACHE_SIZE], 0, MAX_PROCESS_NAME_LEN);
    snprintf(comm_cache->comms[pid % MAX_COMM_CACHE_SIZE], MAX_PROCESS_NAME_LEN, "%s", comm);
    return;
}

bool is_printable_ascii(char c) {
    // Skip ' ' character (32)
    if (c >= 33 && c <= 126)
        return true;
    return false;
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
        log_message(ERROR, __func__, "Unable to malloc for resolved_path");
        return NULL;
    }

    if (realpath(path, resolved_path) == NULL) {
        log_message(ERROR, __func__, "Unable to resolve fullpath of: %s", path);
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

bool is_gte_kernel_version(int major_version, int minor_version, int patch_version) {
    struct utsname buffer;
    
    if (uname(&buffer) != 0) {
        log_message(WARNING, __func__, "Unable to retrieve kernel version! Fanotify masks may be inaccurate!");
        return false; 
    }

    int major, minor, patch;
    if (sscanf(buffer.release, "%d.%d.%d", &major, &minor, &patch) != 3) {
        log_message(WARNING, __func__, "Unable to parse kernel version! Fanotify masks may be inaccurate!");
        return false; 
    }

    if (major > major_version) {
        return true;  // Current kernel is newer
    } else if (major == major_version) {
        if (minor > minor_version) {
            return true;  // Current kernel is newer
        } else if (minor == minor_version) {
            if (patch >= patch_version) {
                return true;  // Current kernel is newer or equal
            }
        }
    }

    return false;  // Current kernel is older
}
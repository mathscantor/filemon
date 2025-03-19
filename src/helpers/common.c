#include "common.h"

/**
 * @brief Retrieves the absolute file path associated with a file descriptor.
 * 
 * This function constructs the `/proc/self/fd/<fd>` symlink and resolves it
 * using `readlink()` to obtain the actual file path.
 * 
 * @param fd File descriptor whose path is to be retrieved.
 * @return char* Pointer to a dynamically allocated string containing the file path,
 *         or NULL if an error occurs. The caller is responsible for freeing the returned string.
 * 
 * @note The function allocates memory for the file path, which must be freed
 *       by the caller to avoid memory leaks.
 * @warning Returns NULL if `fd` is invalid or `readlink()` fails.
 */
char *get_path_from_fd(int fd) {
    ssize_t len;
    char *filepath = (char *)malloc(PATH_MAX);
    char *fd_path = (char *)malloc(PATH_MAX);
    if (fd <= 0) {
        SAFE_FREE(filepath);
        SAFE_FREE(fd_path);
        return NULL;
    }
    snprintf(fd_path, PATH_MAX, "/proc/self/fd/%d", fd);
    if ((len = readlink(fd_path, filepath, PATH_MAX - 1)) < 0) {
        SAFE_FREE(filepath);
        SAFE_FREE(fd_path);
        return NULL;
    }
    filepath[len] = '\0';
    SAFE_FREE(fd_path);
    return filepath;
}

/**
 * @brief Retrieves the command name (comm) of a process given its PID.
 * 
 * This function reads the `/proc/<pid>/comm` file to obtain the command name
 * of a running process. If the process is short-lived and exits before reading,
 * or if an error occurs, the function returns NULL.
 * 
 * @param pid The process ID for which to retrieve the command name.
 * @return char* Pointer to a dynamically allocated string containing the command name,
 *         or NULL if an error occurs. The caller is responsible for freeing the returned string.
 * 
 * @note The function allocates memory for the command name, which must be freed
 *       by the caller to avoid memory leaks.
 * @warning Returns NULL if the process does not exist, if memory allocation fails,
 *          or if the retrieved name contains non-printable ASCII characters.
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
        fclose(comm_file);
        return NULL;
    }

    if (fgets(comm, 16, comm_file)) {
        comm[strcspn(comm, "\n")] = '\0';
    }
    fclose(comm_file);
    
    if (comm[0] == '\0' || comm[0] == ' ') {
        SAFE_FREE(comm_path);
        SAFE_FREE(comm);
        return NULL;
    }

    for (size_t i = 0; i < MAX_PROCESS_NAME_LEN; i++){
        if (comm[i] != '\0' && !is_printable_ascii(comm[i])) {
            SAFE_FREE(comm_path);
            SAFE_FREE(comm);
            return NULL;
        }
    }

    SAFE_FREE(comm_path);
    return comm;
}

/**
 * @brief Retrieves the cached command name (comm) of a process given its PID.
 * 
 * This function checks a pre-populated cache (`comm_cache`) to retrieve 
 * the command name of a process. If the cache does not contain a valid entry 
 * for the given PID, the function returns NULL.
 * 
 * @param pid The process ID for which to retrieve the command name.
 * @param comm_cache Pointer to the command name cache structure.
 * @return char* Pointer to a dynamically allocated string containing the command name,
 *         or NULL if no cached entry exists. The caller is responsible for freeing 
 *         the returned string.
 * 
 * @note This function does not fetch data from `/proc`; it only looks up cached values.
 * @warning Returns NULL if no valid cached entry exists.
 */
char *get_comm_from_cache(uint32_t pid, comm_cache_t *comm_cache) {
   
    if (comm_cache->comms[pid % MAX_COMM_CACHE_SIZE][0] == '\0')
        return NULL;

    return strdup(comm_cache->comms[pid % MAX_COMM_CACHE_SIZE]);
}

/**
 * @brief Stores the command name (comm) of a process in the cache.
 * 
 * This function caches the command name of a process using its PID as an index.
 * If the command name is already present in the cache at the computed index, 
 * the function does nothing. Otherwise, it updates the cache with the new command name.
 * 
 * @param pid The process ID associated with the command name.
 * @param comm The command name to store in the cache.
 * @param comm_cache Pointer to the command name cache structure.
 * 
 * @note The cache index is determined using `pid % MAX_COMM_CACHE_SIZE`, meaning 
 *       multiple processes may overwrite each other's cached values.
 * @warning The function does not allocate new memory; it overwrites existing cached values.
 */
void set_comm_to_cache(uint32_t pid, char *comm, comm_cache_t *comm_cache) {

    if (strcmp(comm, comm_cache->comms[pid % MAX_COMM_CACHE_SIZE]) == 0) {
        // log_message(DEBUG, __func__, "Cache index %d already contains \"%s\". Ignoring...", pid % MAX_COMM_CACHE_SIZE, comm);
        return;
    }
        
    memset(comm_cache->comms[pid % MAX_COMM_CACHE_SIZE], 0, MAX_PROCESS_NAME_LEN);
    snprintf(comm_cache->comms[pid % MAX_COMM_CACHE_SIZE], MAX_PROCESS_NAME_LEN, "%s", comm);
    return;
}

/**
 * @brief Checks if a character is a printable ASCII character.
 * 
 * This function determines whether a given character falls within the 
 * printable ASCII range, excluding the space character (' ').
 * 
 * @param c The character to check.
 * @return true if the character is a printable ASCII character (excluding space), 
 *         false otherwise.
 * 
 * @note Printable ASCII characters range from 33 ('!') to 126 ('~').
 */
bool is_printable_ascii(char c) {
    // Skip ' ' character (32)
    if (c >= 33 && c <= 126)
        return true;
    return false;
}

/**
 * @brief Checks if a given file or directory path exists.
 * 
 * This function uses `stat` to determine whether the specified path exists 
 * in the filesystem.
 * 
 * @param path The path to check.
 * @return true if the path exists, false otherwise.
 * 
 * @note This function does not differentiate between files and directories.
 */
bool path_exists(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) == 0) 
        return true;
    return false;
}

/**
 * @brief Checks if the given path is a directory.
 * 
 * This function uses `stat` to determine whether the specified path 
 * exists and is a directory.
 * 
 * @param path The path to check.
 * @return true if the path is a directory, false otherwise.
 * 
 * @note The function returns false if the path does not exist or is not a directory.
 */
bool is_directory(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
        return true;
    }
    return false;
}

/**
 * @brief Performs a regex search on a given string.
 * 
 * This function executes a compiled regular expression against the 
 * provided `haystack` string to check for a match.
 * 
 * @param expr A pointer to a compiled `regex_t` structure.
 * @param haystack The string to search within.
 * @return true if a match is found, false otherwise.
 * 
 * @note The `regex_t` structure should be properly compiled using `regcomp`
 *       before passing it to this function.
 */
bool regex_search(regex_t *expr, const char* haystack) {

    int ret;
    ret = regexec(expr, haystack, 0, NULL, 0);
    if (!ret) {
        return true;
    }
    return false;
}

/**
 * @brief Resolves the full, absolute path of a given path.
 * 
 * This function uses the `realpath` function to resolve the provided 
 * `path` to its absolute path, taking into account symbolic links, 
 * relative paths, and current working directory.
 * 
 * @param path The input path to resolve.
 * @return A pointer to the resolved full path, or NULL if an error occurs.
 * 
 * @note The returned resolved path should be freed using `free()` when no longer needed.
 * @note If memory allocation or resolution fails, NULL is returned, and an error is logged.
 */
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

/**
 * @brief Converts an array of uint32_t integers to a comma-separated string.
 * 
 * This function takes an array of uint32_t integers and converts it into a string
 * where each element is separated by a comma and a space. The conversion stops when
 * a zero value is encountered in the array. The resulting string is dynamically allocated.
 * 
 * @param arr The array of uint32_t integers to convert.
 * @param arr_len The length of the array.
 * @return A dynamically allocated string containing the comma-separated values of the array, 
 *         or an empty string if the array is NULL, empty, or contains only zeros.
 * 
 * @note The function handles cases where the array contains zeros, and the conversion 
 *       stops when a zero is encountered.
 */
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

/**
 * @brief Checks if a value exists in an array of uint32_t integers.
 * 
 * This function searches for a specified `needle` in the provided `haystack` array of 
 * uint32_t values. It returns `true` if the value is found, and `false` otherwise.
 * 
 * @param haystack The array of uint32_t integers to search through.
 * @param size The number of elements in the array.
 * @param needle The value to search for in the array.
 * @return `true` if the `needle` is found in the array, otherwise `false`.
 */
bool is_in_uint32_array(uint32_t *haystack, size_t size, uint32_t needle) {
    for (size_t i = 0; i < size; i++) {
        if (needle == haystack[i]) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Concatenates an array of process names into a single string.
 * 
 * This function concatenates a given array of process names into a single string, 
 * with each name enclosed in double quotes and separated by commas. It stops 
 * concatenating when it encounters an empty string in the array or reaches the 
 * specified length of the array.
 * 
 * @param arr The array of process names (strings).
 * @param arr_len The number of elements in the array.
 * @return A dynamically allocated string containing the concatenated process names,
 *         or NULL if an error occurs or the input array is empty.
 *         The returned string needs to be freed by the caller.
 */
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

/**
 * @brief Checks if a given process name exists in an array of process names.
 * 
 * This function searches through an array of process names and checks if any of them 
 * match the given needle (process name). It compares the start of each name in the 
 * array with the provided needle.
 * 
 * @param haystack The array of process names (strings).
 * @param size The number of elements in the array.
 * @param needle The process name to search for.
 * @return `true` if the needle is found in the array, `false` otherwise.
 */
bool is_in_process_names(char haystack[][MAX_PROCESS_NAME_LEN], size_t size, char *needle) {
    for (size_t i = 0; i < size; i++) {
        if (strncmp(haystack[i], needle, strlen(needle)) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if the current kernel version is greater than or equal to a specified version.
 * 
 * This function retrieves the current kernel version using `uname` and compares it 
 * to the provided major, minor, and patch version. It returns `true` if the current 
 * kernel version is greater than or equal to the specified version, and `false` otherwise.
 * 
 * @param major_version The major version of the kernel to compare against.
 * @param minor_version The minor version of the kernel to compare against.
 * @param patch_version The patch version of the kernel to compare against.
 * @return `true` if the current kernel version is greater than or equal to the specified version, 
 *         `false` otherwise.
 */
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
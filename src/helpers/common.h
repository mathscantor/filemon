#ifndef HELPERS_COMMON_H
#define HELPERS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
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

#define PATH_MAX 4096
#define MAX_PROCESS_FILTER 64
#define MAX_REGEX_LEN 1024
#define MAX_PROCESS_NAME_LEN 16
#define MAX_COMM_CACHE_SIZE 1024

typedef struct {
    char comms[MAX_COMM_CACHE_SIZE][MAX_PROCESS_NAME_LEN];
} comm_cache_t;

char *get_path_from_fd(int);
char *get_comm_from_pid(uint32_t);
char *get_comm_from_cache(uint32_t, comm_cache_t *);
void set_comm_to_cache(uint32_t , char *, comm_cache_t *);
bool is_printable_ascii(char c);
bool path_exists(const char *);
bool is_directory(const char *);
bool regex_search(regex_t *, const char* );
char *get_full_path(const char *);
bool is_valid_integer(const char *);
char *uint32_array_to_string(const uint32_t *, size_t) ;
bool is_in_uint32_array(uint32_t *, size_t , uint32_t);
char *concatenate_process_names(char [][MAX_PROCESS_NAME_LEN], size_t);
bool is_in_process_names(char [][MAX_PROCESS_NAME_LEN], size_t, char *);

#endif

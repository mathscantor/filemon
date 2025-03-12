#ifndef HELPERS_FANOTIFY_H
#define HELPERS_FANOTIFY_H

#define _GNU_SOURCE

#include <stdlib.h>
#include <sys/fanotify.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>
#include <stdbool.h>
#include <sys/inotify.h>

#include "common.h"
#include "../logging/logger.h"

#define MAX_FLAGS_LEN 128
#define MAX_MASKS_LEN 512

typedef struct {
    bool is_config_fanotify_enabled;
    bool is_config_fanotify_access_permissions_enabled;
    struct {
        int fan_fd;
        uint32_t flags;
        uint64_t masks;
    } read_write_execute;
    struct {
        int fan_fd;
        uint32_t flags;
        uint64_t masks;
    } create_delete_move;
    char mount_path[PATH_MAX];
} fanotify_info_t;

bool has_config_fanotify(void);
bool has_config_fanotify_access_perms(void);
uint32_t fanotify_helper_determine_flags(int, uint64_t, char *);
void fanotify_helper_flags_to_string(uint32_t, char *, size_t);
void fanotify_helper_masks_to_string(uint64_t, char *, size_t);

#endif
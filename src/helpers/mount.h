#ifndef HELPERS_MOUNT_H
#define HELPERS_MOUNT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../logging/logger.h"

#define MAX_MOUNT_POINTS 1024

char *parent_mount_blacklist[] = { "/proc", "/dev", "/snap", "/sys", "/run" };

bool is_relevant_mount_point(char *);
char **get_relevant_mount_points(uint32_t *);
void free_mount_points(char **, uint32_t);

bool is_relevant_mount_point(char *mount) {

    size_t mount_blacklist_size = sizeof(parent_mount_blacklist) / sizeof(char *);

    for (size_t i = 0; i < mount_blacklist_size; i ++) {
        if (strlen(parent_mount_blacklist[i]) > strlen(mount))
            continue;
        if (strncmp(parent_mount_blacklist[i], mount, strlen(parent_mount_blacklist[i])) == 0) {
            return false;
        }
    }
    return true;
}

char **get_relevant_mount_points(uint32_t *num_mounts) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        perror("fopen");
        *num_mounts = 0;
        return NULL;
    }

    char device[256], mount_point[256], fs_type[256], options[256];
    int dump, pass;
    
    char **mount_points = (char **)malloc(MAX_MOUNT_POINTS * sizeof(char*));
    if (!mount_points) {
        log_message(ERROR, 1, __func__, "Unable to allocate memory for mount point buffers!\n");
        fclose(fp);
        *num_mounts = 0;
        return NULL;
    }

    *num_mounts = 0;
    while (fscanf(fp, "%255s %255s %255s %255s %d %d\n", device, mount_point, fs_type, options, &dump, &pass) == 6) {
        
        if (!is_relevant_mount_point(mount_point))
            continue;

        mount_points[*num_mounts] = strdup(mount_point);
        if (!mount_points[*num_mounts]) {
            log_message(ERROR, 1, __func__, " Unable to strdup mount path!\n");
            break;
        }
        (*num_mounts)++;
        if (*num_mounts >= MAX_MOUNT_POINTS) {
            break; 
        }
    }

    fclose(fp);
    return mount_points;
}

void free_mount_points(char **mount_points, uint32_t num_mounts) {
    for (uint32_t i = 0; i < num_mounts; i++) {
        free(mount_points[i]);
    }
    free(mount_points);
}

#endif

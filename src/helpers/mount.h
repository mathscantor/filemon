#ifndef HELPERS_MOUNT_H
#define HELPERS_MOUNT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../logging/logger.h"

#define MAX_MOUNT_POINTS 5
#define OS_MAX_MOUNT_POINTS 1024

bool is_mount_point(char *, char **, size_t);
char **get_all_mount_points(size_t *);
void free_mount_points(char **, uint32_t);

bool is_mount_point(char *mount, char **all_mounts, size_t total_num_mounts) {

    for (size_t i = 0; i < total_num_mounts; i++) {
        if (strlen(all_mounts[i]) != strlen(mount)) 
            continue;
        if (strncmp(all_mounts[i], mount, strlen(mount)) == 0) {
            return true;
        }
    }
    return false;
}

char **get_all_mount_points(size_t *num_mounts) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        log_message(ERROR, __func__, "Error opening \"/proc/mounts\"!");
        *num_mounts = 0;
        return NULL;
    }

    char device[256], mount_point[PATH_MAX], fs_type[256], options[256];
    int dump, pass;
    
    char **mount_points = (char **)malloc(OS_MAX_MOUNT_POINTS * sizeof(char*));
    if (!mount_points) {
        log_message(ERROR, __func__, "Unable to allocate memory for mount point buffers!");
        fclose(fp);
        *num_mounts = 0;
        return NULL;
    }

    *num_mounts = 0;
    while (fscanf(fp, "%255s %4096s %255s %255s %d %d", device, mount_point, fs_type, options, &dump, &pass) == 6) {
        mount_points[*num_mounts] = strdup(mount_point);
        if (!mount_points[*num_mounts]) {
            log_message(ERROR, __func__, " Unable to strdup mount path!");
            break;
        }
        (*num_mounts)++;
        if (*num_mounts >= OS_MAX_MOUNT_POINTS) {
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

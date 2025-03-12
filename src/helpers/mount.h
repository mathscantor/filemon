#ifndef HELPERS_MOUNT_H
#define HELPERS_MOUNT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../helpers/common.h"
#include "../logging/logger.h"

#define MAX_MOUNT_POINTS 5
#define OS_MAX_MOUNT_POINTS 1024

bool is_mount_point(char *, char **, size_t);
char **get_all_mount_points(size_t *);
void free_mount_points(char **, uint32_t);

#endif

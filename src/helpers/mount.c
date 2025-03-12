#include "mount.h"

/**
 * @brief Checks if a given path is a mount point.
 * 
 * This function checks if the provided `mount` path exists within the list of all mount points
 * (`all_mounts`). It compares each entry in the list with the given `mount` and returns `true` if a 
 * match is found, otherwise it returns `false`.
 * 
 * @param mount The path to check if it is a mount point.
 * @param all_mounts An array of strings containing all mount points.
 * @param total_num_mounts The total number of mount points in the `all_mounts` array.
 * @return true If the `mount` path is found in `all_mounts`.
 * @return false If the `mount` path is not found in `all_mounts`.
 */
bool is_mount_point(char *mount, char **all_mounts, size_t total_num_mounts) {

    for (size_t i = 0; i < total_num_mounts; i++) {
        if (strcmp(all_mounts[i], mount) == 0)
            return true;
    }
    return false;
}

/**
 * @brief Retrieves all mount points from the system.
 * 
 * This function reads the `/proc/mounts` file to obtain the list of all mount points in the system.
 * It returns an array of strings, each representing a mount point, and also sets the total number of
 * mount points found. The caller is responsible for freeing the memory allocated for the mount points.
 * 
 * @param num_mounts A pointer to a variable that will hold the number of mount points found.
 * 
 * @return char** A dynamically allocated array of strings containing all mount points. If an error occurs, 
 *         NULL is returned, and `num_mounts` is set to 0.
 */
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

/**
 * @brief Frees the memory allocated for the mount points.
 * 
 * This function frees the memory allocated for each mount point in the `mount_points` array 
 * and then frees the array itself.
 * 
 * @param mount_points The array of mount points to be freed.
 * @param num_mounts The number of mount points in the `mount_points` array.
 */
void free_mount_points(char **mount_points, uint32_t num_mounts) {
    for (uint32_t i = 0; i < num_mounts; i++) {
        SAFE_FREE(mount_points[i]);
    }
    SAFE_FREE(mount_points);
}
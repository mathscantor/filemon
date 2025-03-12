#include "fanotify.h"

/**
 * @brief Checks if CONFIG_FANOTIFY in /boot/config-$(uname -r) is enabled.
 * 
 * @return true if CONFIG_FANOTIFY=y. Otherwise, returns false.
 */
bool has_config_fanotify(void) {
    struct utsname uname_data;
    FILE *file;
    char filepath[256];
    char line[256];
    const char *search_str = "CONFIG_FANOTIFY=y";

    // Get the kernel version
    if (uname(&uname_data) != 0) {
        log_message(ERROR, __func__, "Unable to get kernel version via uname.");
        return false;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        log_message(ERROR, __func__, "Unable to open: \"/boot/config-%s\"", uname_data.release);
        return false;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            fclose(file);
            return true;
        }
    }
    fclose(file);
    return false;
}

/**
 * @brief Checks if CONFIG_FANOTIFY_ACCESS_PERMISSIONS in /boot/config-$(uname -r) is enabled.
 * 
 * @return  true if CONFIG_FANOTIFY_ACCESS_PERMISSIONS=y. Otherwise, returns false.
 */
bool has_config_fanotify_access_perms(void) {
    struct utsname uname_data;
    FILE *file;
    char filepath[256];
    char line[256];
    const char *search_str = "CONFIG_FANOTIFY_ACCESS_PERMISSIONS=y";

    // Get the kernel version
    if (uname(&uname_data) != 0) {
        log_message(ERROR, __func__, "Unable to get kernel version via uname.");
        return false;
    }

    // Construct the file path
    snprintf(filepath, sizeof(filepath), "/boot/config-%s", uname_data.release);

    // Open the file for reading
    file = fopen(filepath, "r");
    if (file == NULL) {
        log_message(ERROR, __func__, "Unable to fopen: %s", filepath);
        return false;
    }

    // Search for the string in the file
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            fclose(file);
            return true;
        }
    }
    fclose(file);
    return false;
}

/**
 * @brief A fanotify_mark() wrapper to account for both newer and older kernel versions.
 * 
 * @param fan_fd The fanotify file descriptor.
 * @param masks The fanotify event masks.
 * @param mount_path The full path to the mount point.
 * @return 0 on failure. Otherwise, returns the actual flags used.
 */
uint32_t fanotify_helper_determine_flags(int fan_fd, uint64_t masks, char *mount_path) {

    uint32_t flags;

    #ifdef FAN_MARK_FILESYSTEM
    /**
     * FANOTIFY_MARK_FILESYSTEM is defined on newer kernels ( > 4.20 ).
     * If filemon was built statically with FANOTIFY_MARK_FILESYSTEM on newer kernel
     * and runs on an older kernel, it will fail to add a mark. Therefore we need to do a check by
     * using FANOTIFY_MARK_FILESYSTEM with read/write/execute masks on the current directory, 
     * which will automatically resolve its mount point. If this fails, fall back to FAN_MARK_MOUNT.
     */
    flags = FAN_MARK_ADD | FAN_MARK_FILESYSTEM;
    if (fanotify_mark(fan_fd, flags, masks, AT_FDCWD, mount_path) == 0) {
        fanotify_mark(fan_fd, FAN_MARK_FLUSH | FAN_MARK_FILESYSTEM, 0, 0, NULL);
        return flags;
    } 
    #endif

    flags = FAN_MARK_ADD | FAN_MARK_MOUNT;
    if (fanotify_mark(fan_fd, flags, masks, AT_FDCWD, mount_path) == 0) {
        fanotify_mark(fan_fd, FAN_MARK_FLUSH | FAN_MARK_MOUNT, 0, 0, NULL);
        return flags;
    }

    return 0;
}

/**
 * @brief Converts the fanotify flags to a string for readability.
 * 
 * @param flags The fanotify flags.
 * @param buf The buffer to store the fanotify flags.
 * @param buf_len The size of the buffer.
 */
void fanotify_helper_flags_to_string(uint32_t flags, char *buf, size_t buf_len) {

    #ifdef FAN_MARK_ADD
    if (flags & FAN_MARK_ADD) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_ADD, "); 
    #endif

    #ifdef FAN_MARK_REMOVE
    if (flags & FAN_MARK_REMOVE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_REMOVE, "); 
    #endif

    #ifdef FAN_MARK_DONT_FOLLOW
    if (flags & FAN_MARK_DONT_FOLLOW) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_DONT_FOLLOW, "); 
    #endif

    #ifdef FAN_MARK_ONLYDIR
    if (flags & FAN_MARK_ONLYDIR) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_ONLYDIR, "); 
    #endif

    #ifdef FAN_MARK_IGNORED_MASK
    if (flags & FAN_MARK_IGNORED_MASK) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_IGNORED_MASK, "); 
    #endif

    #ifdef FAN_MARK_IGNORED_SURV_MODIFY
    if (flags & FAN_MARK_IGNORED_SURV_MODIFY) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_IGNORED_SURV_MODIFY, "); 
    #endif

    #ifdef FAN_MARK_FLUSH
    if (flags & FAN_MARK_FLUSH) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_FLUSH, "); 
    #endif

    #ifdef FAN_MARK_EVICTABLE
    if (flags & FAN_MARK_EVICTABLE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_EVICTABLE, "); 
    #endif

    #ifdef FAN_MARK_IGNORE
    if (flags & FAN_MARK_IGNORE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_IGNORE, "); 
    #endif

    #ifdef FAN_MARK_INODE
    if (flags & FAN_MARK_INODE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_INODE, "); 
    #endif

    #ifdef FAN_MARK_MOUNT
    if (flags & FAN_MARK_MOUNT) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_MOUNT, "); 
    #endif

    #ifdef FAN_MARK_FILESYSTEM
    if (flags & FAN_MARK_FILESYSTEM) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MARK_FILESYSTEM, "); 
    #endif

    buf[strlen(buf) - 2] = '\0';
    return;
}


/**
 * @brief Converts the fanotify masks to a string for readability.
 * 
 * @param masks The fanotify event masks.
 * @param buf The buffer to store the fanotify masks.
 * @param buf_len The size of the buffer.
 */
void fanotify_helper_masks_to_string(uint64_t masks, char *buf, size_t buf_len) {

    #ifdef FAN_ACCESS
    if (masks & FAN_ACCESS) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_ACCESS, ");
    #endif

    #ifdef FAN_ATTRIB
    if (masks & FAN_ATTRIB) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_ATTRIB, ");
    #endif

    #ifdef FAN_MODIFY
    if (masks & FAN_MODIFY) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MODIFY, ");
    #endif

    #ifdef FAN_CLOSE_WRITE
    if (masks & FAN_CLOSE_WRITE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_CLOSE_WRITE, ");
    #endif

    #ifdef FAN_CLOSE_NOWRITE
    if (masks & FAN_CLOSE_NOWRITE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_CLOSE_NOWRITE, ");
    #endif

    #ifdef FAN_OPEN
    if (masks & FAN_OPEN) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_OPEN, ");
    #endif

    #ifdef FAN_MOVED_FROM
    if (masks & FAN_MOVED_FROM) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MOVED_FROM, ");
    #endif

    #ifdef FAN_MOVED_TO
    if (masks & FAN_MOVED_TO) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_MOVED_TO, ");
    #endif

    #ifdef FAN_CREATE
    if (masks & FAN_CREATE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_CREATE, ");
    #endif

    #ifdef FAN_DELETE
    if (masks & FAN_DELETE) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_DELETE, ");
    #endif

    #ifdef FAN_DELETE_SELF
    if (masks & FAN_DELETE_SELF) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_DELETE_SELF, ");
    #endif 

    #ifdef FAN_OPEN_EXEC
    if (masks & FAN_OPEN_EXEC) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_OPEN_EXEC, ");
    #endif

    #ifdef FAN_RENAME
    if (masks & FAN_RENAME) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_RENAME, ");
    #endif

    #ifdef FAN_ONDIR
    if (masks & FAN_ONDIR) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_ONDIR, ");
    #endif

    #ifdef FAN_OPEN_PERM
    if (masks & FAN_OPEN_PERM) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_OPEN_PERM, ");
    #endif

    #ifdef FAN_ACCESS_PERM
    if (masks & FAN_ACCESS_PERM) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_ACCESS_PERM, ");
    #endif

    #ifdef FAN_OPEN_EXEC_PERM
    if (masks & FAN_OPEN_EXEC_PERM) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_OPEN_EXEC_PERM, ");
    #endif

    #ifdef FAN_EVENT_ON_CHILD
    if (masks & FAN_EVENT_ON_CHILD) snprintf(buf + strlen(buf), buf_len - strlen(buf), "FAN_EVENT_ON_CHILD, ");
    #endif

    buf[strlen(buf) - 2] = '\0';
    return;
}
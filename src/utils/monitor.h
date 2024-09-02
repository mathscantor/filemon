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
#include <sys/inotify.h>
#include "wrappers.h"
#include "logger.h"

#ifndef MONITOR_H
#define MONITOR_H

typedef struct {
    int fd_read_write_execute;
    int fd_create_delete_move;
    uint64_t event_mask_create_delete_move;
    uint64_t event_mask_read_write_execute;
    int config_fanotify_enabled;
    int config_fanotify_access_permissions_enabled;
} fanotify_info_t;

typedef struct {
    fanotify_info_t fanotify_info;
    regex_t exclude_regex;
    char parent_path[PATH_MAX];
    char mount_path[PATH_MAX];
    char exclude_pattern[1024];
} monitor_box_t;

typedef struct {
    monitor_box_t* m_box;
} thread_arg_t;

monitor_box_t* init_monitor_box(char* parent_path, char* exclude_pattern);
void begin_monitor(monitor_box_t* m_box);
void stop_monitor(monitor_box_t* m_box);
void print_box(monitor_box_t* m_box);
void apply_fanotify_marks(monitor_box_t* m_box);
void handle_events_read_write_execute(monitor_box_t* m_box);
void handle_events_create_delete_move(monitor_box_t* m_box);
void* handle_create_delete_move_thread(void* arg);
void* handle_read_write_execute_thread(void* arg);

/**
 * @brief 
 * 
 * @param parent_path The parent file path to monitor.
 * @param exclude_pattern Regex pattern to exclude certain path.
 * @return monitor_box_t* 
 */
monitor_box_t* init_monitor_box(char* parent_path, char* exclude_pattern) {

    int ret;

    // Initiliaze and allocate memory properly for the monitor_box_t pointer 
    monitor_box_t* m_box = (monitor_box_t*) malloc(sizeof(monitor_box_t));
    if (m_box == NULL) {
        log_message(ERROR, 1, "Failed to allocate memory to monitoring box\n");
        exit(EXIT_FAILURE);
    }

    // Check if CONFIG_FANOTIFY=y
    if (has_config_fanotify()) {
        // Check if CONFIG_FANOTIFY_ACCESS_PERMS=y
        m_box->fanotify_info.config_fanotify_enabled = 1;
        if (!has_config_fanotify_access_perms()) {
            m_box->fanotify_info.config_fanotify_access_permissions_enabled = 1;
            log_message(WARNING, 1, "Current kernel was built with CONFIG_FANOTIFY_ACCESS_PERMS=n. Not using FAN_*_PERM Flags...\n");
        }
    } else {
        m_box->fanotify_info.config_fanotify_enabled = 0;
        m_box->fanotify_info.config_fanotify_access_permissions_enabled = 0;
        log_message(ERROR, 1, "Either kernel was built with CONFIG_FANOTIFY=n or CONFIG_FANOTIFY option does not exist!\n");
        exit(EXIT_FAILURE);
    }
    
    // Populate the struct with appropriate values
    if (m_box->fanotify_info.config_fanotify_enabled) {
        m_box->fanotify_info.fd_read_write_execute = fanotify_init(FAN_CLOEXEC | FAN_CLASS_CONTENT | FAN_NONBLOCK, O_RDONLY | O_LARGEFILE);
        if (m_box->fanotify_info.fd_read_write_execute == -1) {
            log_message(ERROR, 1, "Failed to fanotify_init(FAN_CLOEXEC | FAN_CLASS_CONTENT | FAN_NONBLOCK, O_RDONLY | O_LARGEFILE\n");
            exit(EXIT_FAILURE);
        } 
        m_box->fanotify_info.event_mask_read_write_execute = (   
            #ifdef FAN_ACCESS
            FAN_ACCESS | 
            #endif

            #ifdef FAN_OPEN
            FAN_OPEN |
            #endif

            #ifdef FAN_MODIFY
            FAN_MODIFY |
            #endif

            #ifdef FAN_OPEN_EXEC
            FAN_OPEN_EXEC |
            #endif

            #ifdef FAN_CLOSE_WRITE
            FAN_CLOSE_WRITE | 
            #endif

            #ifdef FAN_CLOSE_NOWRITE
            FAN_CLOSE_NOWRITE |
            #endif

            FAN_EVENT_ON_CHILD
        ); 
        if (m_box->fanotify_info.config_fanotify_access_permissions_enabled) {
            #ifdef FAN_OPEN_PERM
            m_box->fanotify_info.event_mask_read_write_execute |= FAN_OPEN_PERM;
            #endif
        
            #ifdef FAN_ACCESS_PERM
            m_box->fanotify_info.event_mask_read_write_execute |= FAN_ACCESS_PERM;
            #endif

            #ifdef FAN_OPEN_EXEC_PERM
            m_box->fanotify_info.event_mask_read_write_execute |= FAN_OPEN_EXEC_PERM;
            #endif
        }

        #ifdef FAN_REPORT_DFID_NAME
        m_box->fanotify_info.fd_create_delete_move = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME, O_RDWR);
        if (m_box->fanotify_info.fd_create_delete_move == -1) {
            log_message(ERROR, 1, "Failed to fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME, O_RDWR)\n");
            exit(EXIT_FAILURE);
        }
        m_box->fanotify_info.event_mask_create_delete_move = (
            #ifdef FAN_CREATE
            FAN_CREATE |
            #endif
            
            #ifdef FAN_DELETE
            FAN_DELETE |
            #endif

            #ifdef FAN_RENAME 
            FAN_RENAME |
            #endif 

            #ifdef FAN_MOVED_FROM
            FAN_MOVED_FROM |
            #endif

            #ifdef FAN_MOVED_TO
            FAN_MOVED_TO |
            #endif

            FAN_ONDIR
        );
        #else
        m_box->fanotify_info.fd_create_delete_move = -1;
        m_box->fanotify_info.event_mask_create_delete_move = 0;
        #endif
    }
    
    if (!path_exists(parent_path)) {
        log_message(ERROR, 1, "Directory path does not exist: %s\n", parent_path);
        exit(EXIT_FAILURE);
    }
    if (!is_directory(parent_path)) {
        log_message(ERROR, 1, "Stated path is not a directory: %s\n", parent_path);
        exit(EXIT_FAILURE);
    }
    if (exclude_pattern == NULL) {
        memset(m_box->exclude_pattern, 0, sizeof(m_box->exclude_pattern));
        memset(&m_box->exclude_regex, 0, sizeof(m_box->exclude_regex));
    } else {
        strncpy(m_box->exclude_pattern, exclude_pattern, sizeof(m_box->exclude_pattern));
        ret = regcomp(&m_box->exclude_regex, exclude_pattern, REG_EXTENDED);
        if (ret) {
            log_message(ERROR, 1, "Could not compile exclude_regex\n");
            exit(EXIT_FAILURE);
        }
    }
    strncpy(m_box->parent_path, get_full_path(parent_path), PATH_MAX);

    struct fstab* fs = getfssearch(m_box->parent_path);
    if (fs == NULL) {
        log_message(ERROR, 1, "Could not get mount point of \"%s\" via fstab.\n", m_box->parent_path);
        exit(EXIT_FAILURE);
    }
    strncpy(m_box->mount_path, fs->fs_file, PATH_MAX);
    return m_box;
}

/**
 * @brief Begin monitoring the parent directory specified by the user.
 * 
 * @param m_box The monitor box.
 */
void begin_monitor(monitor_box_t* m_box) {

    #ifdef FAN_REPORT_DFID_NAME
    pthread_t thread1;
    #endif
    pthread_t thread2;
    thread_arg_t args = { .m_box = m_box };

    apply_fanotify_marks(m_box);
    
    // Create the threads
    #ifdef FAN_REPORT_DFID_NAME
    if (pthread_create(&thread1, NULL, handle_create_delete_move_thread, &args) != 0) {
        log_message(ERROR, 1, "Failed to create thread for create/delete events\n");
        exit(EXIT_FAILURE);
    }
    #endif
    if (pthread_create(&thread2, NULL, handle_read_write_execute_thread, &args) != 0) {
        log_message(ERROR, 1, "Failed to create thread for read/write events\n");
        exit(EXIT_FAILURE);
    }
    if (g_logger.logfile[0] != 0) {
        printf("[+] filemon has started successfully.\n");
        printf("[+] All output is redirected to '%s'\n", g_logger.logfile);
    }
    log_message(INFO, 1, "filemon has started successfully.\n");

    #ifdef FAN_REPORT_DFID_NAME
    pthread_join(thread1, NULL);
    #endif
    pthread_join(thread2, NULL);
}

/**
 * @brief Fanotify event handler for read, write and execute events.
 * 
 * @param m_box The monitor box.
 */
void handle_events_read_write_execute(monitor_box_t* m_box) {

    char buf[8192];
    ssize_t buflen;
    struct fanotify_event_metadata *metadata;
    struct fanotify_response response;

    buflen = read(m_box->fanotify_info.fd_read_write_execute, buf, sizeof(buf));
    if (buflen > 0) {
        metadata = (struct fanotify_event_metadata *)buf;
        while (FAN_EVENT_OK(metadata, buflen)) {
            char *comm = get_comm_from_pid(metadata->pid);
            char *full_path = get_path_from_fd(metadata->fd);
            char flags[256];

            if (m_box->fanotify_info.config_fanotify_access_permissions_enabled) {
                #ifdef FAN_OPEN_PERM
                if (metadata->mask & FAN_OPEN_PERM) {
                    response.fd = metadata->fd;
                    response.response = FAN_ALLOW;
                    write(m_box->fanotify_info.fd_read_write_execute, &response, sizeof(response));
                    strncat(flags, "FAN_OPEN_PERM, ", strlen("FAN_OPEN_PERM, ") + 1);
                }
                #endif
                
                #ifdef FAN_ACCESS_PERM
                if (metadata->mask & FAN_ACCESS_PERM) {
                    response.fd = metadata->fd;
                    response.response = FAN_ALLOW;
                    write(m_box->fanotify_info.fd_read_write_execute, &response, sizeof(response));
                    strncat(flags, "FAN_ACCESS_PERM, ", strlen("FAN_ACCESS_PERM, ") + 1);
                }
                #endif

                #ifdef FAN_OPEN_EXEC_PERM
                if (metadata->mask & FAN_OPEN_EXEC_PERM) {
                    response.fd = metadata->fd;
                    response.response = FAN_ALLOW;
                    write(m_box->fanotify_info.fd_read_write_execute, &response, sizeof(response));
                    strncat(flags, "FAN_OPEN_EXEC_PERM, ", strlen("FAN_OPEN_EXEC_PERM, ") + 1);
                }
                #endif
            }

            #ifdef FAN_ACCESS
            if (metadata->mask & FAN_ACCESS) {
                strncat(flags, "FAN_ACCESS, ", strlen("FAN_ACCESS, ") + 1);
            }
            #endif

            #ifdef FAN_OPEN
            if (metadata->mask & FAN_OPEN) {
                strncat(flags, "FAN_OPEN, ", strlen("FAN_OPEN, ") + 1);
            }
            #endif 

            #ifdef FAN_MODIFY
            if (metadata->mask & FAN_MODIFY) {
                strncat(flags, "FAN_MODIFY, ", strlen("FAN_MODIFY, ") + 1);
            }
            #endif

            #ifdef FAN_OPEN_EXEC
            if (metadata->mask & FAN_OPEN_EXEC) {
                strncat(flags, "FAN_OPEN_EXEC, ", strlen("FAN_OPEN_EXEC, ") + 1);
            }
            #endif

            #ifdef FAN_CLOSE_WRITE
            if (metadata->mask & FAN_CLOSE_WRITE) {
                strncat(flags, "FAN_CLOSE_WRITE, ", strlen("FAN_CLOSE_WRITE, ") + 1);
            }
            #endif

            #ifdef FAN_CLOSE_NOWRITE
            if (metadata->mask & FAN_CLOSE_NOWRITE) {
                strncat(flags, "FAN_CLOSE_NOWRITE, ", strlen("FAN_CLOSE_NOWRITE, ") + 1);
            }
            #endif

            if (strncmp(full_path, m_box->parent_path, strlen(m_box->parent_path)) != 0) {
                flags[0] = '\0';
                close(metadata->fd);
                metadata = FAN_EVENT_NEXT(metadata, buflen);
                continue;
            }

            if (m_box->exclude_pattern[0] != 0) {
                if (regex_search(m_box->exclude_regex, full_path)) {
                    flags[0] = '\0';
                    close(metadata->fd);
                    metadata = FAN_EVENT_NEXT(metadata, buflen);
                    continue;
                }
            }
            
            flags[strlen(flags) - 2] = '\0';
            log_message(INFO, 1, "%s (%d): %s == [%s]\n", comm, metadata->pid, full_path, flags);
            flags[0] = '\0';

            // Advance to the next event
            close(metadata->fd);
            metadata = FAN_EVENT_NEXT(metadata, buflen);
        }
    }
    return;
}


#ifdef FAN_REPORT_DFID_NAME
/**
 * @brief Fanotify event handler for create, delete and move events.
 * 
 * @param m_box The monitor box.
 */
void handle_events_create_delete_move(monitor_box_t* m_box) {

    int mount_fd, event_fd;
    char buf[4096];
    unsigned char *file_name;
    struct file_handle *file_handle;
    ssize_t buflen;
    struct fanotify_event_metadata *metadata;
    struct fanotify_event_info_fid *fid;
    char full_path[PATH_MAX];

    buflen = read(m_box->fanotify_info.fd_create_delete_move, buf, sizeof(buf));

    if (buflen > 0) {
        metadata = (struct fanotify_event_metadata*)&buf;
        while (FAN_EVENT_OK(metadata, buflen)) {
            char* comm = get_comm_from_pid(metadata->pid);
            mount_fd = open(m_box->mount_path, O_DIRECTORY | O_RDONLY);
            if (mount_fd == -1) {
                log_message(ERROR, 1, "Failed to open %s\n", m_box->parent_path);
                stop_monitor(m_box);
                exit(EXIT_FAILURE);
            }

            fid = (struct fanotify_event_info_fid *) (metadata + 1);
            file_handle = (struct file_handle *) fid->handle;

            /* Ensure that the event info is of the correct type. */

            if (fid->hdr.info_type == FAN_EVENT_INFO_TYPE_FID || fid->hdr.info_type == FAN_EVENT_INFO_TYPE_DFID) {
                file_name = NULL;
            } else if (fid->hdr.info_type == FAN_EVENT_INFO_TYPE_DFID_NAME) {
                file_name = file_handle->f_handle + file_handle->handle_bytes;
            }

            event_fd = open_by_handle_at(mount_fd, file_handle, O_RDONLY);
            if (event_fd == -1) {
                if (errno == ESTALE) {
                    close(mount_fd);
                    metadata = FAN_EVENT_NEXT(metadata, buflen);
                    continue;
                } else {
                    log_message(ERROR, 1, "Encountered error at open_by_handle_at()\n");
                    exit(EXIT_FAILURE);
                }
            }

            char* path = get_path_from_fd(event_fd);     
            char flags[256];

            if (file_name) {
                snprintf(full_path, sizeof(full_path), "%s/%s", path, file_name);
            } else {
                strncpy(full_path, path, sizeof(full_path));
            }

            #ifdef FAN_CREATE
            if (metadata->mask & FAN_CREATE) {
                strncat(flags, "FAN_CREATE, ", strlen("FAN_CREATE, ") + 1);
                if (metadata->mask & FAN_ONDIR) {
                    strncat(flags, "FAN_ONDIR, ", strlen("FAN_ONDIR, ") + 1);
                }
            }
            #endif

            #ifdef FAN_DELETE
            if (metadata->mask & FAN_DELETE) {
                strncat(flags, "FAN_DELETE, ", strlen("FAN_DELETE, ") + 1);
                if (metadata->mask & FAN_ONDIR) {
                    strncat(flags, "FAN_ONDIR, ", strlen("FAN_ONDIR, ") + 1);
                }
            }
            #endif

            #ifdef FAN_RENAME
            if (metadata->mask & FAN_RENAME) {
                strncat(flags, "FAN_RENAME, ", strlen("FAN_RENAME, ") + 1);
                if (metadata->mask & FAN_ONDIR) {
                    strncat(flags, "FAN_ONDIR, ", strlen("FAN_ONDIR, ") + 1);
                }
            }
            #endif

            #ifdef FAN_MOVED_FROM
            if (metadata->mask & FAN_MOVED_FROM) {
                strncat(flags, "FAN_MOVED_FROM, ", strlen("FAN_MOVED_FROM, ") + 1);
                if (metadata->mask & FAN_ONDIR) {
                    strncat(flags, "FAN_ONDIR, ", strlen("FAN_ONDIR, ") + 1);
                }
            }
            #endif

            #ifdef FAN_MOVED_TO
            if (metadata->mask & FAN_MOVED_TO) {
                strncat(flags, "FAN_MOVED_TO, ", strlen("FAN_MOVED_TO, ") + 1);
                if (metadata->mask & FAN_ONDIR) {
                    strncat(flags, "FAN_ONDIR, ", strlen("FAN_ONDIR, ") + 1);
                }
            }
            #endif

            if (strncmp(full_path, m_box->parent_path, strlen(m_box->parent_path)) != 0) {
                flags[0] = '\0';
                close(metadata->fd);
                close(mount_fd);
                close(event_fd);
                metadata = FAN_EVENT_NEXT(metadata, buflen);
                continue;
            }

            if (m_box->exclude_pattern[0] != 0) {
                if (regex_search(m_box->exclude_regex, full_path)) {
                    flags[0] = '\0';
                    close(metadata->fd);
                    close(mount_fd);
                    close(event_fd);
                    metadata = FAN_EVENT_NEXT(metadata, buflen);
                    continue;
                }
            }

            flags[strlen(flags) - 2] = '\0';
            if (file_name) {
                log_message(INFO, 1, "%s (%d): %s/%s == [%s]\n", comm, metadata->pid, path, file_name, flags);
            } else {
                log_message(INFO, 1, "%s (%d): %s == [%s]\n", comm, metadata->pid, path, flags);
            }
            flags[0] = '\0';
            
            close(metadata->fd);
            close(mount_fd);
            close(event_fd);
            metadata = FAN_EVENT_NEXT(metadata, buflen);
        }
    }

    return;
}
#endif

#ifdef FAN_REPORT_DFID_NAME
/**
 * @brief Thread function to run handle_events_create_delete_move()
 * 
 * @param arg Arguments of handle_events_create_delete_move().
 * @return void* 
 */
void* handle_create_delete_move_thread(void* arg) {
    monitor_box_t* m_box = ((thread_arg_t*)arg)->m_box;
    while (1) {
        handle_events_create_delete_move(m_box);
    }
    return NULL;
}
#endif

/**
 * @brief Thread function to run handle_read_write_execute()
 * 
 * @param arg Arguments of handle_read_write_execute().
 * @return void* 
 */
void* handle_read_write_execute_thread(void* arg) {
    monitor_box_t* m_box = ((thread_arg_t*)arg)->m_box;
    while (1) {
        handle_events_read_write_execute(m_box);
    }
    return NULL;
}

/**
 * @brief Gracefully removes fan marks and free up memory before exiting.
 * 
 * @param m_box The monitor box.
 */
void stop_monitor(monitor_box_t* m_box){
    close(m_box->fanotify_info.fd_read_write_execute);
    close(m_box->fanotify_info.fd_create_delete_move);
    free(m_box);
    if (g_logger.logfile[0] != 0) {
        printf("[+] filemon has stopped successfully\n");
    }
    log_message(INFO, 1, "filemon has stopped successfully.\n");
    return;
}

/**
 * @brief Prints the contents of the monitor box. Just for debugging purposes.
 * 
 * @param m_box The monitor box.
 */
void print_box(monitor_box_t* m_box) {
    log_message(DEBUG, 1, "=============== Monitor Box ==============\n");
    log_message(DEBUG, 1, "Parent Path: %s\n", m_box->parent_path);
    log_message(DEBUG, 1, "Mount Path: %s\n", m_box->mount_path);
    log_message(DEBUG, 1, "Exclude Pattern: %s\n", m_box->exclude_pattern);
    log_message(DEBUG, 1, "Fanotify Read, Write, Execute FD: %d\n", m_box->fanotify_info.fd_read_write_execute);
    log_message(DEBUG, 1, "Fanotify Create, Delete, Move FD: %d\n", m_box->fanotify_info.fd_create_delete_move);
    log_message(DEBUG, 1, "===========================================\n");
    return;
}

/**
 * @brief FAN_MARK_ADD recursively from path.
 * 
 * @param m_box The monitor box.
 */
void apply_fanotify_marks(monitor_box_t* m_box) {

    int ret;

    #ifdef FAN_MARK_FILESYSTEM
    int mark_mode = FAN_MARK_ADD | FAN_MARK_FILESYSTEM;
    #else
    int mark_mode = FAN_MARK_ADD | FAN_MARK_MOUNT;
    #endif

    ret = fanotify_mark(m_box->fanotify_info.fd_read_write_execute, mark_mode, m_box->fanotify_info.event_mask_read_write_execute, AT_FDCWD, m_box->mount_path);
    if (ret == -1) {
        log_message(ERROR, 1, "Failed to apply fanotify mark (event_mask_read_write_execute) on \"%s\" mount\n", m_box->mount_path);
        exit(EXIT_FAILURE);
    }
    log_message(DEBUG, 1, "Successfully applied fanotify mark (event_mask_read_write_execute) on \"%s\" mount\n", m_box->mount_path);

    #ifdef FAN_REPORT_DFID_NAME
    ret = fanotify_mark(m_box->fanotify_info.fd_create_delete_move, mark_mode, m_box->fanotify_info.event_mask_create_delete_move, AT_FDCWD, m_box->mount_path);
    if (ret == -1) {
        log_message(ERROR, 1, "Failed to apply fanotify mark (event_mask_create_delete_move) on \"%s\" mount\n", m_box->mount_path);
        exit(EXIT_FAILURE);
    }
    log_message(DEBUG, 1, "Successfully applied fanotify mark (event_mask_create_delete_move) on \"%s\" mount\n", m_box->mount_path);
    #endif
}
#endif


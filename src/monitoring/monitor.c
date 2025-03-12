#include "monitor.h"

bool g_monitor_force_stop = false;
pthread_mutex_t g_log_mutex;

void init_monitor_box(monitor_box_t *m_box, user_args_t *user_args, char *mount_path) {

    *m_box = (monitor_box_t) {
        .enable_perms_check = user_args->oopts_enable_perms_check,
        .fanotify_info = {
            .is_config_fanotify_enabled = has_config_fanotify(),
            .is_config_fanotify_access_permissions_enabled = has_config_fanotify_access_perms(),
            .read_write_execute = {
                .fan_fd = -1,
                .flags = 0,
                .masks = 0
            },
            .create_delete_move = {
                .fan_fd = -1,
                .flags = 0,
                .masks = 0
            },
            .mount_path = {0}
        },
        .filters = {
            .include_pids = {0},
            .exclude_pids = {0},
            .include_process = {{0}},
            .exclude_process = {{0}},
            .include_path_pattern = {0},
            .include_path_regex = NULL,
            .exclude_path_pattern = {0},
            .exclude_path_regex = NULL
        },
        .comm_cache = {
            .comms = {{0}}
        }
    };

    if (!m_box->fanotify_info.is_config_fanotify_enabled) {
        log_message(ERROR, __func__, "Kernel does not have fanotify capabiltiies!");
        return;
    }

    //Poppulate the monitor box
    memcpy(m_box->fanotify_info.mount_path, mount_path, PATH_MAX);
    memcpy(m_box->filters.include_pids, user_args->oopts_include_pids, sizeof(user_args->oopts_include_pids));
    memcpy(m_box->filters.exclude_pids, user_args->oopts_exclude_pids, sizeof(user_args->oopts_exclude_pids));
    memcpy(m_box->filters.include_process, user_args->oopts_include_process, sizeof(user_args->oopts_include_process));
    memcpy(m_box->filters.exclude_process, user_args->oopts_exclude_process, sizeof(user_args->oopts_exclude_process));
    memcpy(m_box->filters.include_path_pattern, user_args->oopts_include_path_pattern, sizeof(user_args->oopts_include_path_pattern));
    m_box->filters.include_path_regex = user_args->oopts_include_path_regex;
    memcpy(m_box->filters.exclude_path_pattern, user_args->oopts_exclude_path_pattern, sizeof(user_args->oopts_exclude_path_pattern));
    m_box->filters.exclude_path_regex = user_args->oopts_exclude_path_regex;
    
    m_box->fanotify_info.read_write_execute.fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_CONTENT, O_RDONLY | O_LARGEFILE);
    if (m_box->fanotify_info.read_write_execute.fan_fd == -1) {
        return;
    }

    #ifdef FAN_EVENT_ON_CHILD
    m_box->fanotify_info.read_write_execute.masks |= FAN_EVENT_ON_CHILD;
    #endif

    #ifdef FAN_ONDIR
    m_box->fanotify_info.read_write_execute.masks |= FAN_ONDIR;
    #endif

    #ifdef FAN_ACCESS
    m_box->fanotify_info.read_write_execute.masks |= FAN_ACCESS;
    #endif

    #ifdef FAN_OPEN
    m_box->fanotify_info.read_write_execute.masks |= FAN_OPEN;
    #endif

    #ifdef FAN_MODIFY
    m_box->fanotify_info.read_write_execute.masks |= FAN_MODIFY;
    #endif

    #ifdef FAN_OPEN_EXEC
    m_box->fanotify_info.read_write_execute.masks |= FAN_OPEN_EXEC;
    #endif

    #ifdef FAN_CLOSE_WRITE
    m_box->fanotify_info.read_write_execute.masks |= FAN_CLOSE_WRITE;
    #endif

    #ifdef FAN_CLOSE_NOWRITE
    m_box->fanotify_info.read_write_execute.masks |= FAN_CLOSE_NOWRITE;
    #endif

    if (m_box->fanotify_info.is_config_fanotify_access_permissions_enabled && m_box->enable_perms_check) {
        #ifdef FAN_OPEN_PERM
        m_box->fanotify_info.read_write_execute.masks |= FAN_OPEN_PERM;
        #endif
    
        #ifdef FAN_ACCESS_PERM
        m_box->fanotify_info.read_write_execute.masks |= FAN_ACCESS_PERM;
        #endif

        #ifdef FAN_OPEN_EXEC_PERM
        m_box->fanotify_info.read_write_execute.masks |= FAN_OPEN_EXEC_PERM;
        #endif
    }

    m_box->fanotify_info.read_write_execute.flags = fanotify_helper_determine_flags(m_box->fanotify_info.read_write_execute.fan_fd, 
                                                                                    m_box->fanotify_info.read_write_execute.masks, 
                                                                                    mount_path);

    #ifdef FAN_REPORT_DFID_NAME
    m_box->fanotify_info.create_delete_move.fan_fd = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME, O_RDWR);
    if (m_box->fanotify_info.create_delete_move.fan_fd  == -1) {
        return;
    }
    
    #ifdef FAN_ONDIR
    m_box->fanotify_info.create_delete_move.masks |= FAN_ONDIR;
    #endif

    #ifdef FAN_CREATE
    m_box->fanotify_info.create_delete_move.masks |= FAN_CREATE;
    #endif
        
    #ifdef FAN_DELETE
    m_box->fanotify_info.create_delete_move.masks |= FAN_DELETE;
    #endif

    #ifdef FAN_RENAME 
    m_box->fanotify_info.create_delete_move.masks |= FAN_RENAME;
    #endif 

    #ifdef FAN_MOVED_FROM
    m_box->fanotify_info.create_delete_move.masks |= FAN_MOVED_FROM;
    #endif

    #ifdef FAN_MOVED_TO
    m_box->fanotify_info.create_delete_move.masks |= FAN_MOVED_TO;
    #endif

    #ifdef FAN_ATTRIB
    m_box->fanotify_info.create_delete_move.masks |= FAN_ATTRIB;
    #endif

    m_box->fanotify_info.create_delete_move.flags = fanotify_helper_determine_flags(m_box->fanotify_info.create_delete_move.fan_fd, 
                                                                                    m_box->fanotify_info.create_delete_move.masks, 
                                                                                    mount_path);
    #endif  
    return;
}

/**
 * @brief Begin monitoring the parent directory specified by the user.
 * 
 * @param m_box The monitor box.
 */
void begin_monitor(monitor_box_t **m_boxes, size_t num_boxes) {

    int ret;
    size_t num_running_threads = 0;

    pthread_mutex_init(&g_log_mutex, NULL);
    pthread_t *monitoring_threads = (pthread_t *)malloc(num_boxes * 2 * sizeof(pthread_t));
    for (size_t i = 0; i < num_boxes; i++) {
        print_box(m_boxes[i], i);
        thread_arg_t args = { .m_box = m_boxes[i] };

        ret = fanotify_mark(m_boxes[i]->fanotify_info.read_write_execute.fan_fd, 
            m_boxes[i]->fanotify_info.read_write_execute.flags, 
            m_boxes[i]->fanotify_info.read_write_execute.masks,
            AT_FDCWD, 
            m_boxes[i]->fanotify_info.mount_path);
        if (ret == 0) {
            log_message(DEBUG, __func__, "Spawning thread to monitor read/write/execute events on \"%s\" mount...", m_boxes[i]->fanotify_info.mount_path);
            if (pthread_create(&monitoring_threads[i], NULL, handle_rwe_events_thread, &args) != 0) {
                log_message(WARNING, __func__, "Unable to create thread to monitor read/write/execute events on \"%s\" mount", m_boxes[i]->fanotify_info.mount_path);
            } else {
                num_running_threads++;
            }
        } else {
            log_message(WARNING, __func__, "Unable to fanotify_mark with read/write/execute masks on \"%s\" mount!", m_boxes[i]->fanotify_info.mount_path);
        }
        
        #ifdef FAN_REPORT_DFID_NAME
        ret = fanotify_mark(m_boxes[i]->fanotify_info.create_delete_move.fan_fd, 
            m_boxes[i]->fanotify_info.create_delete_move.flags, 
            m_boxes[i]->fanotify_info.create_delete_move.masks, 
            AT_FDCWD, 
            m_boxes[i]->fanotify_info.mount_path);
        if (ret == 0) {
            log_message(DEBUG, __func__, "Spawning thread to monitor create/delete/move events on \"%s\" mount...", m_boxes[i]->fanotify_info.mount_path);
            if (pthread_create(&monitoring_threads[i + 1], NULL, handle_cdm_events_thread, &args) != 0) {
                log_message(WARNING, __func__, "Unable to create thread to monitor create/delete/move events on \"%s\" mount", m_boxes[i]->fanotify_info.mount_path);
            } else {
                num_running_threads++;
            }
        } else {
            log_message(WARNING, __func__, "Unable to fanotify_mark with create/delete/move masks on \"%s\" mount!", m_boxes[i]->fanotify_info.mount_path);
        }
        #endif
    }

    if (g_logger.log_file.f) {
        printf("[+] Successfully started filemon.\n");
        printf("[+] All output is redirected to \"%s\"\n", g_logger.log_file.fullpath);
    }

    if (num_running_threads > 0) {
        log_message(INFO, __func__, "Successfully started filemon (Monitoring threads: %lu).", num_running_threads);
        for (size_t i = 0; i < num_running_threads; i++) {
            pthread_join(monitoring_threads[i], NULL);
        }
    }
    else {
        log_message(ERROR, __func__, "There are 0 monitoring threads! Failed to start filemon!");
    }

    return;
}

void* handle_rwe_events_thread(void* arg) {

    monitor_box_t* m_box = ((thread_arg_t*)arg)->m_box;
    struct pollfd pfd;

    pfd.fd = m_box->fanotify_info.read_write_execute.fan_fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, -1);
    if (ret <= 0) {
        log_message(ERROR, __func__, "Error polling fanotify FD (%d) for read/write/execute events on \"%s\" mount!", 
            m_box->fanotify_info.read_write_execute.fan_fd,
            m_box->fanotify_info.mount_path);
        exit(EXIT_FAILURE);
    }
    for (;;) {
        if (pfd.revents & POLLIN) {
            handle_rwe_events(m_box);
        }
    }
    return NULL;
}

/**
 * @brief Fanotify event handler for read, write and execute events.
 * 
 * @param m_box The monitor box.
 */
void handle_rwe_events(monitor_box_t *m_box) {

    char buf[8192];
    ssize_t buflen;
    struct fanotify_event_metadata *metadata;
    struct fanotify_response response;
    char masks[MAX_MASKS_LEN] = {0};

    char *comm = NULL;
    char *full_path = NULL;

    buflen = read(m_box->fanotify_info.read_write_execute.fan_fd, buf, sizeof(buf));
    if (buflen > 0) {
        metadata = (struct fanotify_event_metadata *)buf;
        while (FAN_EVENT_OK(metadata, buflen)) {

            /* Process name search with caching logic */
            comm = get_comm_from_pid((uint32_t)metadata->pid);
            if (comm != NULL) {
                set_comm_to_cache((uint32_t)metadata->pid, comm, &m_box->comm_cache);
                // log_message(DEBUG, __func__, "Set comm to cache: %s", comm);
            } else {
                comm = get_comm_from_cache((uint32_t)metadata->pid, &m_box->comm_cache);
                // log_message(DEBUG, __func__, "Got comm from cache: %s", comm);
            }
            if (comm == NULL) 
                comm = strdup("unknown-process");
            
            full_path = get_path_from_fd(metadata->fd);
            if (m_box->fanotify_info.is_config_fanotify_access_permissions_enabled && m_box->enable_perms_check)  {

                #ifdef FAN_OPEN_PERM
                if (metadata->mask & FAN_OPEN_PERM) goto write_fanotify_response;
                #endif
                
                #ifdef FAN_ACCESS_PERM
                if (metadata->mask & FAN_ACCESS_PERM) goto write_fanotify_response;
                #endif

                #ifdef FAN_OPEN_EXEC_PERM
                if (metadata->mask & FAN_OPEN_EXEC_PERM) goto write_fanotify_response;
                #endif

write_fanotify_response:
                response.fd = metadata->fd;
                response.response = FAN_ALLOW;
                write(m_box->fanotify_info.read_write_execute.fan_fd, &response, sizeof(response));
            }

            // Ignore self
            if (metadata->pid == getpid()) goto next_event;

            /* Apply Filters */
            if (m_box->filters.include_pids[0] != 0) {
                if (!is_in_uint32_array(m_box->filters.include_pids, MAX_PROCESS_FILTER, (uint32_t)metadata->pid)) {
                    goto next_event;
                }
            } else if (m_box->filters.exclude_pids[0] != 0) {
                if (is_in_uint32_array(m_box->filters.exclude_pids, MAX_PROCESS_FILTER, (uint32_t)metadata->pid)) {
                    goto next_event;
                }
            }

            if (m_box->filters.include_process[0][0] != 0) {
                if (!is_in_process_names(m_box->filters.include_process, MAX_PROCESS_FILTER, comm)) {
                    goto next_event;
                }
            } else if (m_box->filters.exclude_process[0][0] != 0) {
                if (is_in_process_names(m_box->filters.exclude_process, MAX_PROCESS_FILTER, comm)) {
                    goto next_event;
                }
            }

            if (m_box->filters.include_path_regex != NULL) {
                if (!regex_search(m_box->filters.include_path_regex, full_path)) {
                    goto next_event;
                }
            } else if (m_box->filters.exclude_path_regex != NULL) {
                if (regex_search(m_box->filters.exclude_path_regex, full_path)) {
                    goto next_event;
                }
            }
            
            fanotify_helper_masks_to_string(metadata->mask, masks, MAX_MASKS_LEN);
            pthread_mutex_lock(&g_log_mutex);
            log_message(INFO, __func__, "%s (%d): %s == [%s]", comm, metadata->pid, full_path, masks);
            pthread_mutex_unlock(&g_log_mutex);

next_event:
            memset(masks, 0, sizeof(masks));
            close(metadata->fd);
            SAFE_FREE(full_path);
            SAFE_FREE(comm);
            metadata = FAN_EVENT_NEXT(metadata, buflen);
        }
    }

    return;
}

#ifdef FAN_REPORT_DFID_NAME

/**
 * @brief Thread function to run handle_events_create_delete_move()
 * 
 * @param arg Arguments of handle_events_create_delete_move().
 * @return void* 
 */
void* handle_cdm_events_thread(void* arg) {
    monitor_box_t* m_box = ((thread_arg_t*)arg)->m_box;
    struct pollfd pfd;

    pfd.fd = m_box->fanotify_info.create_delete_move.fan_fd;
    pfd.events = POLLIN;
    
    int ret = poll(&pfd, 1, -1);
    if (ret <= 0) {
        log_message(ERROR, __func__, "Error polling fanotify FD (%d) for create/delete/move events on \"%s\" mount!", 
            m_box->fanotify_info.create_delete_move.fan_fd,
            m_box->fanotify_info.mount_path);
        exit(EXIT_FAILURE);
    }
    for (;;) {
        if (pfd.revents & POLLIN) {
            handle_cdm_events(m_box);
        }
    }
    return NULL;
}

/**
 * @brief Fanotify event handler for create, delete and move events.
 * 
 * @param m_box The monitor box.
 */
void handle_cdm_events(monitor_box_t* m_box) {

    int mount_fd, event_fd;
    char buf[4096];
    unsigned char *file_name;
    struct file_handle *file_handle;
    ssize_t buflen;
    struct fanotify_event_metadata *metadata;
    struct fanotify_event_info_fid *fid;
    char full_path[PATH_MAX];
    char masks[MAX_MASKS_LEN] = {0};
    char *comm = NULL;

    buflen = read(m_box->fanotify_info.create_delete_move.fan_fd, buf, sizeof(buf));

    if (buflen > 0) {
        metadata = (struct fanotify_event_metadata*)&buf;
        while (FAN_EVENT_OK(metadata, buflen)) {
            
            /* Process name search up with caching logic */
            comm = get_comm_from_pid((uint32_t)metadata->pid);
            if (comm != NULL) {
                set_comm_to_cache((uint32_t)metadata->pid, comm, &m_box->comm_cache);
                // log_message(DEBUG, __func__, "Set comm to cache: %s", comm);
            } else {
                comm = get_comm_from_cache((uint32_t)metadata->pid, &m_box->comm_cache);
                // log_message(DEBUG, __func__, "Got comm from cache: %s", comm);
            }
            if (comm == NULL) {
                comm = strdup("unknown-process");
            }

            mount_fd = open(m_box->fanotify_info.mount_path, O_DIRECTORY | O_RDONLY);
            if (mount_fd == -1) {
                log_message(WARNING, __func__, "Unable to open \"%s\" mount!", m_box->fanotify_info.mount_path);
                sleep(5);
                return;
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
                    log_message(ERROR, __func__, "Encountered error at open_by_handle_at");
                    exit(EXIT_FAILURE);
                }
            }

            char* path = get_path_from_fd(event_fd);     

            if (file_name) {
                snprintf(full_path, PATH_MAX, "%s/%s", path, file_name);
            } else {
                snprintf(full_path, PATH_MAX, "%s", path);
            }

            // Ignore self
            if (metadata->pid == getpid()) goto next_event;

            /* Apply Filters */
            if (m_box->filters.include_pids[0] != 0) {
                if (!is_in_uint32_array(m_box->filters.include_pids, MAX_PROCESS_FILTER, (uint32_t)metadata->pid)) {
                    goto next_event;
                }
            } else if (m_box->filters.exclude_pids[0] != 0) {
                if (is_in_uint32_array(m_box->filters.exclude_pids, MAX_PROCESS_FILTER, (uint32_t)metadata->pid)) {
                    goto next_event;
                }
            }

            if (m_box->filters.include_process[0][0] != 0) {
                if (!is_in_process_names(m_box->filters.include_process, MAX_PROCESS_FILTER, comm)) {
                    goto next_event;
                }
            } else if (m_box->filters.exclude_process[0][0] != 0) {
                if (is_in_process_names(m_box->filters.exclude_process, MAX_PROCESS_FILTER, comm)) {
                    goto next_event;
                }
            }

            if (m_box->filters.include_path_regex != NULL) {
                if (!regex_search(m_box->filters.include_path_regex, full_path)) {
                    goto next_event;
                }
            } else if (m_box->filters.exclude_path_regex != NULL) {
                if (regex_search(m_box->filters.exclude_path_regex, full_path)) {
                    goto next_event;
                }
            }

            fanotify_helper_masks_to_string(metadata->mask, masks, MAX_MASKS_LEN);
            pthread_mutex_lock(&g_log_mutex);
            log_message(INFO, __func__, "%s (%d): %s == [%s]", comm, metadata->pid, full_path, masks);
            pthread_mutex_unlock(&g_log_mutex);

next_event:
            memset(masks, 0, sizeof(masks));
            close(metadata->fd);
            close(mount_fd);
            close(event_fd);
            SAFE_FREE(path);
            SAFE_FREE(comm);
            metadata = FAN_EVENT_NEXT(metadata, buflen);
        }
    }

    return;
}
#endif


void stop_monitor(monitor_box_t **m_boxes, size_t num_boxes){

    if (g_logger.log_file.f) {
        printf("[+] Stopping filemon...\n");
    }
    log_message(INFO, __func__, "Stopping filemon...");

    pthread_mutex_destroy(&g_log_mutex);
    for (size_t i = 0; i < num_boxes; i++) {
        fanotify_mark(m_boxes[i]->fanotify_info.read_write_execute.fan_fd, FAN_MARK_FLUSH, 0, 0, NULL);
        SAFE_FREE(m_boxes[i]);
    }
    if (g_logger.log_file.f) {
        printf("[+] Successfully stopped filemon.\n");
        printf("[+] To view the logs: less -R \"%s\"\n", g_logger.log_file.fullpath);
    }
    log_message(INFO, __func__, "Successfully stopped filemon!");
    return;
}

/**
 * @brief Prints the contents of the monitor box. Just for debugging purposes.
 * 
 * @param m_box The monitor box.
 */
void print_box(monitor_box_t* m_box, uint32_t index) {

    char buf_read_write_execute_flags[MAX_FLAGS_LEN] = {0};
    char buf_read_write_execute_masks[MAX_MASKS_LEN] = {0};
    char buf_create_delete_move_flags[MAX_FLAGS_LEN] = {0};
    char buf_create_delete_move_masks[MAX_MASKS_LEN] = {0}; 

    fanotify_helper_flags_to_string(m_box->fanotify_info.read_write_execute.flags, buf_read_write_execute_flags, MAX_FLAGS_LEN);
    fanotify_helper_masks_to_string(m_box->fanotify_info.read_write_execute.masks, buf_read_write_execute_masks, MAX_MASKS_LEN);
    fanotify_helper_flags_to_string(m_box->fanotify_info.create_delete_move.flags, buf_create_delete_move_flags, MAX_FLAGS_LEN);
    fanotify_helper_masks_to_string(m_box->fanotify_info.create_delete_move.masks, buf_create_delete_move_masks, MAX_MASKS_LEN);

    char *buf_include_pids = uint32_array_to_string(m_box->filters.include_pids, MAX_PROCESS_FILTER);
    char *buf_exclude_pids = uint32_array_to_string(m_box->filters.exclude_pids, MAX_PROCESS_FILTER);

    char *buf_include_process = concatenate_process_names(m_box->filters.include_process, MAX_PROCESS_FILTER);
    char *buf_exclude_process = concatenate_process_names(m_box->filters.exclude_process, MAX_PROCESS_FILTER);

    log_message(DEBUG, __func__, "Monitor Box %u: { enable_perms_check = %d, fanotify_info = { is_config_fanotify_enabled = %d, is_config_fanotify_access_permissions_enabled = %d, read_write_execute = { fan_fd = %d, flags = %u (%s), masks = %u (%s) }, create_delete_move = { fan_fd = %d, flags = %u (%s), masks = %u (%s) }, mount_path = \"%s\" }, filters = { include_pids = {%s}, exclude_pids = {%s}, include_process = {%s}, exclude_process = {%s}, include_path_regex = \"%s\", exclude_path_regex = \"%s\" } }",
    index, m_box->enable_perms_check, 
    m_box->fanotify_info.is_config_fanotify_enabled, 
    m_box->fanotify_info.is_config_fanotify_access_permissions_enabled,
    m_box->fanotify_info.read_write_execute.fan_fd,
    m_box->fanotify_info.read_write_execute.flags, buf_read_write_execute_flags,
    m_box->fanotify_info.read_write_execute.masks, buf_read_write_execute_masks,
    m_box->fanotify_info.create_delete_move.fan_fd,
    m_box->fanotify_info.create_delete_move.flags, buf_create_delete_move_flags,
    m_box->fanotify_info.create_delete_move.masks, buf_create_delete_move_masks,
    m_box->fanotify_info.mount_path,
    buf_include_pids,
    buf_exclude_pids,
    buf_include_process,
    buf_exclude_process,
    m_box->filters.include_path_pattern,
    m_box->filters.exclude_path_pattern);

    SAFE_FREE(buf_include_pids);
    SAFE_FREE(buf_exclude_pids);
    SAFE_FREE(buf_include_process);
    SAFE_FREE(buf_exclude_process);
    return;
}
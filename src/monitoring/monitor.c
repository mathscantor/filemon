#include "monitor.h"

bool g_monitor_force_stop = false;

pthread_mutex_t g_log_mutex;
pthread_t *monitoring_threads = NULL;
size_t num_running_threads = 0;

/**
 * @brief Initializes the monitor box with the provided arguments.
 * 
 * This function sets up a monitor box (`monitor_box_t`) to monitor a specific mount 
 * point using `fanotify` and applies various filters such as process IDs and path patterns. 
 * The function also determines the appropriate event masks based on the kernel version 
 * and configuration options.
 * 
 * @param m_box A pointer to the monitor box (`monitor_box_t`) to be initialized.
 * @param user_args A pointer to the user arguments (`user_args_t`) that specify configuration options.
 * @param mount_path The mount point path to monitor.
 */
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
            .exclude_path_regex = NULL,
            .events = {NULL}
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
    if (mount_path != NULL)
        memcpy(m_box->fanotify_info.mount_path, mount_path, PATH_MAX);
    if (user_args->oopts_include_pids[0] != 0)
        memcpy(m_box->filters.include_pids, user_args->oopts_include_pids, sizeof(user_args->oopts_include_pids));
    if (user_args->oopts_exclude_pids[0] != 0)
        memcpy(m_box->filters.exclude_pids, user_args->oopts_exclude_pids, sizeof(user_args->oopts_exclude_pids));
    if (user_args->oopts_include_process[0][0] != 0)
        memcpy(m_box->filters.include_process, user_args->oopts_include_process, sizeof(user_args->oopts_include_process));
    if (user_args->oopts_exclude_process[0][0] != 0)
        memcpy(m_box->filters.exclude_process, user_args->oopts_exclude_process, sizeof(user_args->oopts_exclude_process));
    if (user_args->oopts_include_path_regex != NULL) {
        memcpy(m_box->filters.include_path_pattern, user_args->oopts_include_path_pattern, sizeof(user_args->oopts_include_path_pattern));
        m_box->filters.include_path_regex = user_args->oopts_include_path_regex;
    }
    if (user_args->oopts_exclude_path_regex != NULL) {
        memcpy(m_box->filters.exclude_path_pattern, user_args->oopts_exclude_path_pattern, sizeof(user_args->oopts_exclude_path_pattern));
        m_box->filters.exclude_path_regex = user_args->oopts_exclude_path_regex;
    }
    for (size_t i = 0; i < MAX_EVENT_FILTERS; i++) {
        if (user_args->oopts_events[i] == NULL) 
            break;
        m_box->filters.events[i] = strdup(user_args->oopts_events[i]);
    }

    #ifdef FAN_EVENT_ON_CHILD
    m_box->fanotify_info.read_write_execute.masks |= FAN_EVENT_ON_CHILD;
    #endif

    #ifdef FAN_ONDIR
    m_box->fanotify_info.read_write_execute.masks |= FAN_ONDIR;
    #endif

    // Filter according to specified events
    if (m_box->filters.events[0] != NULL) {
        for (size_t i = 0; i < MAX_EVENT_FILTERS; i++) {
            if (m_box->filters.events[i] == NULL)
                break;
            if (strcmp(m_box->filters.events[i], "read") == 0) {

                #ifdef FAN_CLOSE_NOWRITE
                m_box->fanotify_info.read_write_execute.masks |= FAN_CLOSE_NOWRITE;
                #endif

            } else if (strcmp(m_box->filters.events[i], "write") == 0) {

                #ifdef FAN_CLOSE_WRITE
                m_box->fanotify_info.read_write_execute.masks |= FAN_CLOSE_WRITE;
                #endif

                #ifdef FAN_MODIFY
                m_box->fanotify_info.read_write_execute.masks |= FAN_MODIFY;
                #endif

            } else if (strcmp(m_box->filters.events[i], "execute") == 0) {

                #ifdef FAN_OPEN_EXEC
                /* According to fanotify man page - FAN_OPEN_EXEC (since Linux 5.0) */
                if (is_gte_kernel_version(5, 0, 0, 0))
                    m_box->fanotify_info.read_write_execute.masks |= FAN_OPEN_EXEC;
                else {
                    log_message(ERROR, __func__, "FAN_OPEN_EXEC mask is not supported in your current kernel version!");
                    exit(EXIT_FAILURE);
                }
                #endif

            }
        }
        goto determine_rwe_flags;
    }

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
    /* According to fanotify man page - FAN_OPEN_EXEC (since Linux 5.0) */
    if (is_gte_kernel_version(5, 0, 0, 0))
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
        /* According to fanotify man page - FAN_OPEN_EXEC_PERM (since Linux 5.0) */
        if (is_gte_kernel_version(5, 0, 0, 0))
            m_box->fanotify_info.read_write_execute.masks |= FAN_OPEN_EXEC_PERM;
        #endif
    }

determine_rwe_flags:
    m_box->fanotify_info.read_write_execute.fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_CONTENT, O_RDONLY | O_LARGEFILE);
    if (m_box->fanotify_info.read_write_execute.fan_fd == -1) {
        return;
    }
    m_box->fanotify_info.read_write_execute.flags = fanotify_helper_determine_flags(m_box->fanotify_info.read_write_execute.fan_fd, 
                                                                                    m_box->fanotify_info.read_write_execute.masks, 
                                                                                    mount_path);

    #ifdef FAN_REPORT_DFID_NAME
    
    #ifdef FAN_ONDIR
    m_box->fanotify_info.create_delete_move.masks |= FAN_ONDIR;
    #endif
    
    // Filter according to specified events
    if (m_box->filters.events[0] != NULL) {
        for (size_t i = 0; i < MAX_EVENT_FILTERS; i++) {
            if (m_box->filters.events[i] == NULL)
                break;
            if (strcmp(m_box->filters.events[i], "create") == 0) {

                #ifdef FAN_CREATE
                /* According to fanotify man page - FAN_CREATE (since Linux 5.1) */
                if (is_gte_kernel_version(5, 1, 0, 0))
                    m_box->fanotify_info.create_delete_move.masks |= FAN_CREATE;
                else {
                    log_message(ERROR, __func__, "FAN_CREATE mask is not supported in your current kernel version!");
                    exit(EXIT_FAILURE);
                }
                #endif

            } else if (strcmp(m_box->filters.events[i], "delete") == 0) {

                #ifdef FAN_DELETE
                /* According to fanotify man page - FAN_DELETE (since Linux 5.1) */
                if (is_gte_kernel_version(5, 1, 0, 0))
                    m_box->fanotify_info.create_delete_move.masks |= FAN_DELETE;
                else {
                    log_message(ERROR, __func__, "FAN_DELETE mask is not supported in your current kernel version!");
                    exit(EXIT_FAILURE);
                }
                #endif

            } else if (strcmp(m_box->filters.events[i], "move") == 0) {

                #ifdef FAN_MOVED_FROM
                /* According to fanotify man page - FAN_MOVED_FROM (since Linux 5.1) */
                if (is_gte_kernel_version(5, 1, 0, 0))
                    m_box->fanotify_info.create_delete_move.masks |= FAN_MOVED_FROM;
                else  {
                    log_message(ERROR, __func__, "FAN_MOVED_FROM mask is not supported in your current kernel version!");
                    exit(EXIT_FAILURE);
                }
                #endif

                #ifdef FAN_MOVED_TO
                /* According to fanotify man page - FAN_MOVED_TO (since Linux 5.1) */
                if (is_gte_kernel_version(5, 1, 0, 0))
                    m_box->fanotify_info.create_delete_move.masks |= FAN_MOVED_TO;
                else  {
                    log_message(ERROR, __func__, "FAN_MOVED_TO mask is not supported in your current kernel version!");
                    exit(EXIT_FAILURE);
                }
                #endif
        
            }
        }
        goto determine_cdm_flags;
    }

    #ifdef FAN_CREATE
    /* According to fanotify man page - FAN_CREATE (since Linux 5.1) */
    if (is_gte_kernel_version(5, 1, 0, 0))
        m_box->fanotify_info.create_delete_move.masks |= FAN_CREATE;
    #endif
        
    #ifdef FAN_DELETE
    /* According to fanotify man page - FAN_DELETE (since Linux 5.1) */
    if (is_gte_kernel_version(5, 1, 0, 0))
        m_box->fanotify_info.create_delete_move.masks |= FAN_DELETE;
    #endif

    #ifdef FAN_RENAME 
    /* According to fanotify man page - FAN_RENAME (since Linux 5.17, 5.15.154, and 5.10.220) */
    if (is_gte_kernel_version(5, 17, 0, 0) || is_gte_kernel_version(5, 15, 154, 0) || is_gte_kernel_version(5, 10, 220, 0))
        m_box->fanotify_info.create_delete_move.masks |= FAN_RENAME;
    #endif 

    #ifdef FAN_MOVED_FROM
    /* According to fanotify man page - FAN_MOVED_FROM (since Linux 5.1) */
    if (is_gte_kernel_version(5, 1, 0, 0))
        m_box->fanotify_info.create_delete_move.masks |= FAN_MOVED_FROM;
    #endif

    #ifdef FAN_MOVED_TO
    /* According to fanotify man page - FAN_MOVED_TO (since Linux 5.1) */
    if (is_gte_kernel_version(5, 1, 0, 0))
        m_box->fanotify_info.create_delete_move.masks |= FAN_MOVED_TO;
    #endif

    #ifdef FAN_ATTRIB
    /* According to fanotify man page - FAN_ATTRIB (since Linux 5.1) */
    if (is_gte_kernel_version(5, 1, 0, 0))
        m_box->fanotify_info.create_delete_move.masks |= FAN_ATTRIB;
    #endif

determine_cdm_flags:
    m_box->fanotify_info.create_delete_move.fan_fd = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME, O_RDWR);
    if (m_box->fanotify_info.create_delete_move.fan_fd  == -1) {
        return;
    }
    m_box->fanotify_info.create_delete_move.flags = fanotify_helper_determine_flags(m_box->fanotify_info.create_delete_move.fan_fd, 
                                                                                    m_box->fanotify_info.create_delete_move.masks, 
                                                                                    mount_path);
    #endif
    return;
}

/**
 * @brief Starts monitoring file events using `fanotify` for the specified monitor boxes.
 * 
 * This function initializes monitoring for multiple mounts by spawning threads to 
 * handle `read/write/execute` (RWE) and `create/delete/move` (CDM) events. It 
 * configures the `fanotify` event masks for each mount, and starts separate threads 
 * to monitor the events on each mount path. It also handles the logging of the process.
 * 
 * @param m_boxes A pointer to an array of monitor box pointers (`monitor_box_t`) to be monitored.
 * @param num_boxes The number of monitor boxes to be monitored.
 */
void begin_monitor(monitor_box_t **m_boxes, size_t num_boxes) {

    int ret;

    pthread_mutex_init(&g_log_mutex, NULL);
    monitoring_threads = (pthread_t *)malloc(num_boxes * 2 * sizeof(pthread_t));
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

/**
 * @brief Handles read, write, and execute events in a separate thread for a given monitor box.
 * 
 * This function is intended to be run in a thread that continuously monitors read, write, 
 * and execute events on a specific mount path using `fanotify`. It waits for events on 
 * the specified `fanotify` file descriptor and processes them using the `handle_rwe_events` function.
 * If the polling operation fails or encounters an error, it logs the error and terminates the program.
 * 
 * @param arg A pointer to a `thread_arg_t` structure that contains the monitor box to monitor.
 * 
 * @return NULL This function does not return any value, as it runs in an infinite loop to handle events.
 */
void *handle_rwe_events_thread(void* arg) {

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
 * @brief Processes read, write, and execute events from fanotify and applies filters.
 * 
 * This function reads events from the fanotify file descriptor associated with the 
 * monitor box, processes each event, and applies the specified filters. If the event 
 * matches the filters, it logs the event and applies permissions checks if configured. 
 * It also caches the process names for further use to avoid redundant lookups.
 * If fanotify permissions are enabled and the event requires a response, it sends an 
 * approval response back to the kernel to allow the action.
 * 
 * @param m_box A pointer to the monitor box (`monitor_box_t`) that contains fanotify information 
 *              and filters to apply to the events.
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

            // Ignore if full_path is NULL
            if (full_path == NULL) goto next_event;

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
 * @brief Handles create, delete, and move events from fanotify in a separate thread.
 * 
 * This function polls the fanotify file descriptor associated with the create, delete, and move 
 * events of the given monitor box. When events are available, it passes the monitor box to 
 * the `handle_cdm_events` function for processing. It continuously listens for events until the 
 * program is terminated or an error occurs while polling.
 * 
 * @param arg A pointer to a `thread_arg_t` structure that contains the monitor box (`monitor_box_t`) 
 *            for which events will be handled.
 * 
 * @return NULL
 */
void *handle_cdm_events_thread(void* arg) {
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
 * @brief Handles create, delete, and move events from fanotify.
 * 
 * This function processes events related to file creation, deletion, and movement that are 
 * received from the fanotify system. It reads events from the fanotify file descriptor, 
 * processes each event, and applies various filters to decide whether the event should be logged. 
 * If necessary, it also retrieves the full path of the file affected by the event and logs relevant 
 * information about the event, including the process that triggered it.
 * 
 * @param m_box A pointer to a `monitor_box_t` structure representing the mount and fanotify 
 *              configuration for the monitored events.
 *
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

            char *path = get_path_from_fd(event_fd);   
            if (path == NULL) goto next_event;  

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

/**
 * @brief Stops the file monitoring process and cleans up resources.
 * 
 * This function stops the file monitoring process by destroying mutexes, flushing fanotify marks, 
 * and freeing the allocated memory for each monitor box. It also logs the stop event and provides 
 * feedback if the logger file is active.
 * 
 * @param m_boxes A pointer to an array of monitor box pointers representing the monitored file systems.
 * @param num_boxes The number of monitor boxes to stop monitoring.
 * 
 */
void stop_monitor(monitor_box_t **m_boxes, size_t num_boxes){

    if (g_logger.log_file.f) {
        printf("[+] Stopping filemon...\n");
    }
    log_message(INFO, __func__, "Stopping filemon...");

    pthread_mutex_destroy(&g_log_mutex);
    for (size_t i = 0; i < num_running_threads; i++) {
        pthread_cancel(monitoring_threads[i]);
    }
    SAFE_FREE(monitoring_threads);

    for (size_t i = 0; i < num_boxes; i++) {
        fanotify_mark(m_boxes[i]->fanotify_info.read_write_execute.fan_fd, FAN_MARK_FLUSH, 0, 0, NULL);
        SAFE_FREE(m_boxes[i]);
    }
    SAFE_FREE(m_boxes);

    if (g_logger.log_file.f) {
        printf("[+] Successfully stopped filemon.\n");
        printf("[+] To view the logs: less -R \"%s\"\n", g_logger.log_file.fullpath);
    }
    log_message(INFO, __func__, "Successfully stopped filemon!");
    return;
}

/**
 * @brief Prints the details of a monitor box.
 * 
 * This function prints the current configuration and settings of a monitor box. It includes the fanotify
 * flags, masks, and various filter settings related to process IDs, process names, and path patterns. The 
 * function logs these details for the specified monitor box at the provided index.
 * 
 * @param m_box A pointer to the monitor box whose details are to be printed.
 * @param index The index of the monitor box in the list of monitor boxes.
 * 
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
    index + 1, m_box->enable_perms_check, 
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
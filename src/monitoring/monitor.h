#ifndef MONITORING_MONITOR_H
#define MONITORING_MONITOR_H

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
#include <poll.h>
#include <linux/version.h>

#include "../args/parser.h"
#include "../helpers/common.h"
#include "../helpers/fanotify.h"
#include "../logging/logger.h"

typedef struct {
    uint32_t include_pids[MAX_PROCESS_FILTER];
    uint32_t exclude_pids[MAX_PROCESS_FILTER];
    char include_process[MAX_PROCESS_FILTER][MAX_PROCESS_NAME_LEN];
    char exclude_process[MAX_PROCESS_FILTER][MAX_PROCESS_NAME_LEN];
    char include_path_pattern[MAX_REGEX_LEN];
    regex_t *include_path_regex;
    char exclude_path_pattern[MAX_REGEX_LEN];
    regex_t *exclude_path_regex;
    char *events[MAX_EVENT_FILTERS];
} filters_t;

typedef struct {
    bool enable_perms_check;
    fanotify_info_t fanotify_info;
    filters_t filters;
    comm_cache_t comm_cache;
    dev_t mount_dev;
    bool mount_dev_valid;
} monitor_box_t;

typedef struct {
    monitor_box_t* m_box;
} thread_arg_t;

extern bool g_monitor_force_stop;

void init_monitor_box(monitor_box_t *,user_args_t *, char *);
void begin_monitor(monitor_box_t **, size_t);
void stop_monitor(monitor_box_t **, size_t);
void print_box(monitor_box_t *, uint32_t);
void handle_rwe_events(monitor_box_t *);
void *handle_rwe_events_thread(void *);

#ifdef FAN_REPORT_DFID_NAME
void handle_cdm_events(monitor_box_t *);
void *handle_cdm_events_thread(void *);
#endif

#endif


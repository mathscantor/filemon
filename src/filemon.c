#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/fanotify.h>
#include <signal.h>
#include <limits.h>

#include "monitoring/monitor.h"
#include "logging/logger.h"
#include "args/parser.h"
#include "helpers/common.h"
#include "helpers/mount.h"

monitor_box_t **m_boxes;
uint32_t num_mounts = 0;

void sigint_handler();
void usage();

int main(int argc, char *argv[]) {

    user_args_t user_args = parse_args(argc, argv);

    if (user_args.oopts_output)
        logger_init(user_args.oopts_verbose, user_args.oopts_output); 
    else
        logger_init(user_args.oopts_verbose, NULL); 

    // Assert root EUID
    __u32 euid = geteuid();
    if (euid != 0) {
        log_message(ERROR, 1, __func__, "Please run this as root!\n");
        exit(EXIT_FAILURE);
    }

    if (user_args.oopts_output) {
        printf("[+] Starting filemon...\n");
    }
    log_message(INFO, 1, __func__, "Starting filemon...\n");

    // Set up signal handler for SIGINT
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        log_message(ERROR, 1, __func__, " Failed to set up signal handler\n"); // TODO __func__
        exit(EXIT_FAILURE);
    }

    char **mounts = get_relevant_mount_points(&num_mounts);
    if (!mounts) {
        log_message(ERROR, 1, __func__, "Failed to set up signal handler\n");
        exit(EXIT_FAILURE);
    }

    m_boxes = (monitor_box_t **)malloc(num_mounts * sizeof(monitor_box_t *));
    for (uint32_t i = 0; i < num_mounts; i++) {
        m_boxes[i] = (monitor_box_t *)malloc(sizeof(monitor_box_t));
        init_monitor_box(m_boxes[i], &user_args, mounts[i]);
    }
    begin_monitor(m_boxes, num_mounts);

    return 0;
}

/**
 * @brief SIGINT handler to call other functions.
 * 
 * @param signum 
 */
void sigint_handler() {
    stop_monitor(m_boxes, num_mounts);
    exit(EXIT_SUCCESS);
}


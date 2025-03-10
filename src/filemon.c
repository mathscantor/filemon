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
struct sigaction action;
user_args_t user_args;

void signal_handler(int);

int main(int argc, char *argv[]) {

    user_args = parse_args(argc, argv);

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

    // Set up signal handler
    action.sa_handler = signal_handler;
    if (sigaction(SIGINT, &action, 0) == -1) {
        log_message(ERROR, 1, __func__, " Failed to set up SIGINT handler\n"); 
        exit(EXIT_FAILURE);
    }

    m_boxes = (monitor_box_t **)malloc(user_args.oopts_num_mounts * sizeof(monitor_box_t *));
    for (uint32_t i = 0; i < user_args.oopts_num_mounts; i++) {
        m_boxes[i] = (monitor_box_t *)malloc(sizeof(monitor_box_t));
        init_monitor_box(m_boxes[i], &user_args, user_args.oopts_mounts[i]);
    }
    begin_monitor(m_boxes, user_args.oopts_num_mounts);

    return 0;
}

/**
 * @brief SIGINT handler to call other functions.
 * 
 * @param signum 
 */
void signal_handler(int sig) {

    switch(sig) {
        case SIGINT:
            log_message(DEBUG, 1, __func__, "Signal %d - SIGINT received!\n", sig);
            stop_monitor(m_boxes, user_args.oopts_num_mounts);
            exit(EXIT_SUCCESS);
            break;
        default:
            log_message(DEBUG, 1, __func__, "Unknown Signal %d received!\n", sig);
    }

}


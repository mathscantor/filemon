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

/**
 * @brief The entry point of the filemon program.
 * 
 * This function initializes logging, verifies that the program is being run as root, 
 * sets up the signal handler for SIGINT, and initializes the monitoring process 
 * for the specified mounts. It allocates memory for the monitor boxes, 
 * sets up each monitor box with user arguments, and starts the monitoring process.
 * 
 * @param argc The number of command-line arguments passed to the program.
 * @param argv The array of command-line arguments.
 * 
 * @return int Returns 0 on successful execution, or exits the program if an error occurs.
 */
int main(int argc, char *argv[]) {

    user_args = parse_args(argc, argv);

    if (user_args.oopts_output)
        logger_init(user_args.oopts_verbose, user_args.oopts_output); 
    else
        logger_init(user_args.oopts_verbose, NULL); 

    // Assert root EUID
    __u32 euid = geteuid();
    if (euid != 0) {
        log_message(ERROR, __func__, "Please run this as root!");
        exit(EXIT_FAILURE);
    }

    if (user_args.oopts_output) {
        printf("[+] Starting filemon...\n");
    }
    log_message(INFO, __func__, "Starting filemon...");

    // Set up signal handler
    action.sa_handler = signal_handler;
    if (sigaction(SIGINT, &action, 0) == -1) {
        log_message(ERROR, __func__, " Failed to set up SIGINT handler"); 
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
 * @brief Handles incoming signals, including SIGINT.
 * 
 * This function is used to handle various signals received by the program. Currently, it handles 
 * the SIGINT signal, logging the receipt of the signal and gracefully stopping the monitoring process 
 * before exiting the program. For any other signals, a message is logged indicating an unknown signal.
 * 
 * @param sig The signal number received.
 *
 */
void signal_handler(int sig) {

    switch(sig) {
        case SIGINT:
            log_message(DEBUG, __func__, "Signal %d - SIGINT received!", sig);
            stop_monitor(m_boxes, user_args.oopts_num_mounts);
            exit(EXIT_SUCCESS);
            break;
        default:
            log_message(DEBUG, __func__, "Unknown Signal %d received!", sig);
    }
    return;
}


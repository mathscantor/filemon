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
#include <getopt.h>

#include "utils/monitor.h"
#include "utils/wrappers.h"
#include "utils/logger.h"

void sigint_handler();
void usage();

monitor_box_t* m_box = NULL;

int main(int argc, char* argv[]) {

    // Define long options
    struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"exclude-pattern", required_argument, 0, 'e'},
        {"log", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    // Arguments Default Values
    int oopts_verbose = 1;
    char *oopts_exclude_pattern = NULL;
    char *oopts_logfile = NULL;
    char *posarg_directory = NULL;

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hve:l:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                oopts_verbose = 2; 
                break;
            case 'e':
                oopts_exclude_pattern = optarg;
                break;
            case 'l':
                oopts_logfile = optarg;
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    // Check if directory is provided
    if (optind < argc) {
        posarg_directory = argv[optind];
    } else {
        usage();
        exit(EXIT_FAILURE);
    }

    logger_init(oopts_verbose, oopts_logfile); 
    if (oopts_logfile) {
        printf("[+] Starting filemon...\n");
    }
    log_message(INFO, 1, "Starting filemon...\n");

    // Assert root EUID
    __u32 euid = geteuid();
    if (euid != 0) {
        log_message(ERROR, 1, "Please run this as root!\n");
        exit(EXIT_FAILURE);
    }

    // Set up signal handler for SIGINT
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        log_message(ERROR, 1, "Failed to set up signal handler\n");
        exit(EXIT_FAILURE);
    }

    // Check if CONFIG_FANOTIFY=y
    if (!has_config_fanotify()) {
        log_message(ERROR, 1, "Either kernel was built with CONFIG_FANOTIFY=n or CONFIG_FANOTIFY option does not exist!\n");
        exit(EXIT_FAILURE);
    }

    // Check if CONFIG_FANOTIFY_ACCESS_PERMS=y
    if (!has_config_fanotify_access_perms()) {
        log_message(WARNING, 1, "Current kernel was built with CONFIG_FANOTIFY_ACCESS_PERMS=n. Disabling permission hooks\n");
    }

    m_box = init_monitor_box(posarg_directory, oopts_exclude_pattern);
    print_box(m_box);    
    begin_monitor(m_box);

    return 0;
}

/**
 * @brief SIGINT handler to call other functions.
 * 
 * @param signum 
 */
void sigint_handler() {
    if (g_logger.logfile[0] != 0) {
        printf("[+] Stopping filemon...\n");
    }
    log_message(INFO, 1, "Stopping filemon...\n");
    stop_monitor(m_box);
    exit(EXIT_SUCCESS);
}

/**
 * @brief Prints the usage of the program
 * 
 */
void usage(){
    printf("Usage: filemon [-h|--help] [-v] [-e PATTERN] [-l LOGFILE] DIRECTORY\n");
    printf("%-30s %s\n", "-h  | --help", "Show help");
    printf("%-30s %s\n", "-v  | --verbose", "Enables debug logs.");
    printf("%-30s %s\n", "-e  | --exclude-pattern", "Ignore events when path matches regex pattern.");
    printf("%-30s %s\n", "-l  | --log", "The path of the log file.");
    return;
}

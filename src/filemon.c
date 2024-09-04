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
        {"output", required_argument, 0, 'o'},
        {"mount", required_argument, 0, 'm'},
        {"include-pids", required_argument, 0, 'L'},
        {0, 0, 0, 0}
    };

    // Arguments Default Values
    int oopts_verbose = 1;
    char *oopts_exclude_pattern = NULL;
    char *oopts_output = NULL;
    char *oopts_mount = NULL;
    int oopts_include_pids[FILTER_MAX];
    memset(oopts_include_pids, 0, sizeof(oopts_include_pids));
    char *posarg_directory = NULL;

    int opt;
    int option_index = 0;
    char* token;
    int i = 0;
    while ((opt = getopt_long(argc, argv, "hve:o:m:L:", long_options, &option_index)) != -1) {
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
            case 'o':
                oopts_output = optarg;
                break;
            case 'm':
                oopts_mount = optarg;
                break;
            case 'L':
                token = strtok(optarg, " ");
                i = 0;
                if (oopts_include_pids[0] != 0) {
                    log_message(ERROR, 1, "%c option: Cannot be used more than once.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                while (token != NULL) {
                    if (!is_valid_integer(token)) {
                        log_message(ERROR, 1, "%c option: '%s' is not an integer.\n", opt, token);
                        exit(EXIT_FAILURE);
                    } 
                    oopts_include_pids[i] = atoi(token);
                    i++;
                    token = strtok(NULL, " ");
                }
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

    logger_init(oopts_verbose, oopts_output); 
    if (oopts_output) {
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

    m_box = init_monitor_box(posarg_directory, oopts_mount, oopts_include_pids, oopts_exclude_pattern);
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
    printf("Usage: filemon [-h|--help] [-v] [-e PATTERN] [-l LOGFILE] [-m MOUNT] [-L INCLUDE_PIDS] DIRECTORY\n");
    printf("%-30s %s\n", "-h  | --help", "Show help");
    printf("%-30s %s\n", "-v  | --verbose", "Enables debug logs.");
    printf("%-30s %s\n", "-e  | --exclude-pattern", "Ignore events when path matches regex pattern.");
    printf("%-30s %s\n", "-o  | --output", "The path of the log file.");
    printf("%-30s %s\n", "-m  | --mount", "The mount path. (Use this option to override auto search from fstab)");
    printf("%-30s %s\n", "-L  | --include-pids", "Only show events related to these pids. (Eg. -L \"4728 4279\")");
    return;
}

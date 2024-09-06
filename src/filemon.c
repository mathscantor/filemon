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
        {"include-pattern", required_argument, 0, 'i'},
        {"exclude-pattern", required_argument, 0, 'e'},
        {"output", required_argument, 0, 'o'},
        {"mount", required_argument, 0, 'm'},
        {"include-pids", required_argument, 0, 'I'},
        {"enclude-pids", required_argument, 0, 'E'},
        {"include-process", required_argument, 0, 'N'},
        {"exclude-process", required_argument, 0, 'X'},
        {0, 0, 0, 0}
    };

    // Arguments Default Values
    int oopts_verbose = 1;
    char* oopts_include_pattern = NULL;
    char* oopts_exclude_pattern = NULL;
    char* oopts_output = NULL;
    char* oopts_mount = NULL;
    
    int oopts_include_pids[FILTER_MAX];
    memset(oopts_include_pids, 0, sizeof(oopts_include_pids));
    
    int oopts_exclude_pids[FILTER_MAX];
    memset(oopts_exclude_pids, 0, sizeof(oopts_exclude_pids));
    
    char** oopts_include_process = malloc(FILTER_MAX * sizeof(char *));
    for (int i = 0; i < FILTER_MAX; i++) {
        oopts_include_process[i] = NULL;
    }
    char** oopts_exclude_process = malloc(FILTER_MAX * sizeof(char *));
    for (int i = 0; i < FILTER_MAX; i++) {
        oopts_exclude_process[i] = NULL;
    }
    
    char *posarg_directory = NULL;

    int opt;
    int option_index = 0;
    char* token;
    int i = 0;
    while ((opt = getopt_long(argc, argv, "hvi:e:o:m:I:E:N:X:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                oopts_verbose = 2; 
                break;
            case 'i':
                if (oopts_exclude_pattern){
                    log_message(ERROR, 1, "-%c option: Cannot be used with -e option at the same time.\n", opt);
                    exit(EXIT_FAILURE);
                }
                if (oopts_include_pattern){
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                }
                oopts_include_pattern = optarg;
                break;
            case 'e':
                if (oopts_include_pattern){
                    log_message(ERROR, 1, "-%c option: Cannot be used with -i option at the same time.\n", opt);
                    exit(EXIT_FAILURE);
                }
                if (oopts_exclude_pattern){
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                }
                oopts_exclude_pattern = optarg;
                break;
            case 'o':
                if (oopts_output) {
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                }
                oopts_output = optarg;
                break;
            case 'm':
                if (oopts_mount) {
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                }
                oopts_mount = optarg;
                break;
            case 'I':
                token = strtok(optarg, " ");
                i = 0;
                if (oopts_exclude_pids[0] != 0) {
                    log_message(ERROR, 1, "-%c option: Cannot be used with -E option at the same time.\n", opt);
                    exit(EXIT_FAILURE);
                }             
                if (oopts_include_pids[0] != 0) {
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                } 
                while (token != NULL) {
                    if (!is_valid_integer(token)) {
                        log_message(ERROR, 1, "-%c option: '%s' is not an integer.\n", opt, token);
                        exit(EXIT_FAILURE);
                    } 
                    oopts_include_pids[i] = atoi(token);
                    i++;
                    token = strtok(NULL, " ");
                }
                break;
            case 'E':
                token = strtok(optarg, " ");
                i = 0;
                if (oopts_include_pids[0] != 0) {
                    log_message(ERROR, 1, "-%c option: Cannot be used with -I option at the same time.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                if (oopts_exclude_pids[0] != 0) {
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                while (token != NULL) {
                    if (!is_valid_integer(token)) {
                        log_message(ERROR, 1, "%c option: '%s' is not an integer.\n", opt, token);
                        exit(EXIT_FAILURE);
                    } 
                    oopts_exclude_pids[i] = atoi(token);
                    i++;
                    token = strtok(NULL, " ");
                }
                break;
            case 'N':
                token = strtok(optarg, " ");
                i = 0;
                if (oopts_exclude_process[0]) {
                    log_message(ERROR, 1, "-%c option: Cannot be used with -X option at the same time.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                if (oopts_include_process[0]) {
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                while (token != NULL) {
                    oopts_include_process[i] = token;
                    i++;
                    token = strtok(NULL, " ");
                }
                break;
            case 'X':
                token = strtok(optarg, " ");
                i = 0;
                if (oopts_include_process[0]) {
                    log_message(ERROR, 1, "-%c option: Cannot be used with -N option at the same time.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                if (oopts_exclude_process[0]) {
                    log_message(ERROR, 1, "-%c option: Cannot be used more than once.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                while (token != NULL) {
                    oopts_exclude_process[i] = token;
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

    if (oopts_output)
        logger_init(oopts_verbose, get_full_path(oopts_output)); 
    else
        logger_init(oopts_verbose, NULL); 

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

    m_box = init_monitor_box(posarg_directory, oopts_mount, 
                            oopts_include_pids, oopts_exclude_pids, 
                            oopts_include_process, oopts_exclude_process,
                            oopts_include_pattern, oopts_exclude_pattern);
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
    printf("Usage: filemon [-h] [-v] [-o OUTPUT] [-m MOUNT]\n" 
    "%15s[-i INCLUDE_PATERN | -e EXCLUDE_PATTERN] [-I INCLUDE_PIDS | -E EXCLUDE_PIDS] DIRECTORY\n", "");
    printf("Options:\n");
    printf("  %-30s %s\n", "-h  | --help", "Show help");
    printf("  %-30s %s\n", "-v  | --verbose", "Enables debug logs.");
    printf("  %-30s %s\n", "-i  | --include-pattern", "Only show events when path matches regex pattern.");
    printf("  %-30s %s\n", "-e  | --exclude-pattern", "Ignore events when path matches regex pattern.");
    printf("  %-30s %s\n", "-o  | --output", "Output to file");
    printf("  %-30s %s\n", "-m  | --mount", "The mount path. (Use this option to override auto search from fstab)");
    printf("  %-30s %s\n", "-I  | --include-pids", "Only show events related to these pids. (Eg. -I \"4728 4279\")");
    printf("  %-30s %s\n", "-E  | --exclude-pids", "Ignore events related to these pids. (Eg. -E \"6728 6817\")");
    printf("  %-30s %s\n", "-N  | --include-process", "Only show events related to these process names. (Eg. -N \"python3 systemd\")");
    printf("  %-30s %s\n", "-X  | --exclude-process", "Ignore events related to these process names. (Eg. -X \"python3 systemd\")");
    return;
} 

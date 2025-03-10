#ifndef ARGS_PARSER_H
#define ARGS_PARSER_H

#include <getopt.h>
#include <string.h>
#include <regex.h>
#include <stdint.h>

#include "../helpers/common.h"
#include "../helpers/mount.h"

typedef struct {

    /* Verboisty Level */
    uint16_t oopts_verbose;

    /* Target Mounts */
    char oopts_mounts[MAX_MOUNT_POINTS][PATH_MAX];
    size_t oopts_num_mounts;

    /* Include Path Regex Pattern */
    char oopts_include_path_pattern[MAX_REGEX_LEN];
    regex_t *oopts_include_path_regex;

    /* Exclude Path Regex Pattern */
    char oopts_exclude_path_pattern[MAX_REGEX_LEN];
    regex_t *oopts_exclude_path_regex;

    /* Output File */
    char *oopts_output;

    /* Include PIDs */
    uint32_t oopts_include_pids[MAX_PROCESS_FILTER];

    /* Exclude PIDs */
    uint32_t oopts_exclude_pids[MAX_PROCESS_FILTER];

    /* Include Proccess Names */
    char oopts_include_process[MAX_PROCESS_FILTER][MAX_PROCESS_NAME_LEN];

    /* Exclude Process Names*/
    char oopts_exclude_process[MAX_PROCESS_FILTER][MAX_PROCESS_NAME_LEN];

    /* Enable Permission Checks */
    bool oopts_enable_perms_check;
} user_args_t;

user_args_t parse_args(int, char* []);
void usage(void);

user_args_t parse_args(int argc, char* argv[]) {

    int opt;
    int option_index = 0;
    char* token;
    size_t total_num_mounts;

    user_args_t user_args = {
        .oopts_verbose = 1,
        .oopts_mounts = { "/", "", "", "", "" },
        .oopts_num_mounts = 1,
        .oopts_include_path_pattern = {0},
        .oopts_include_path_regex = NULL,
        .oopts_exclude_path_pattern = {0},
        .oopts_exclude_path_regex = NULL,
        .oopts_output = NULL,
        .oopts_include_pids = {0},
        .oopts_exclude_pids = {0},
        .oopts_include_process = {{0}},
        .oopts_exclude_process = {{0}},
        .oopts_enable_perms_check =  false
    };    

    struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"mounts", required_argument, 0, 'm'},
        {"include-path-pattern", required_argument, 0, 'i'},
        {"exclude-path-pattern", required_argument, 0, 'e'},
        {"output", required_argument, 0, 'o'},
        {"include-pids", required_argument, 0, 'I'},
        {"enclude-pids", required_argument, 0, 'E'},
        {"include-process", required_argument, 0, 'N'},
        {"exclude-process", required_argument, 0, 'X'},
        {"enable-perms-check", no_argument, 0, 'P'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hvm:i:e:o:I:E:N:X:P", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                user_args.oopts_verbose = 2; 
                break;
            case 'm':
                if (optarg[0] == '\0') {
                    log_message(ERROR, 1, __func__, "-%c option: No mount points were specified! Please state at least one!\n", opt);
                    exit(EXIT_FAILURE);
                }

                char **all_mounts = get_all_mount_points(&total_num_mounts);
                if (!all_mounts) {
                    log_message(ERROR, 1, __func__, "-%c option: Failed to get relevant mount points!\n", opt);
                    exit(EXIT_FAILURE);
                }
                user_args.oopts_num_mounts = 0;
                token = strtok(optarg, " ");
                for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
                    if (token == NULL) 
                        break;
                    if (!is_mount_point(token, all_mounts, total_num_mounts)) {
                        log_message(ERROR, 1, __func__, "-%c option: \"%s\" is not a mount point!\n", opt, token);
                        exit(EXIT_FAILURE);
                    }
                    snprintf(user_args.oopts_mounts[i], PATH_MAX, "%s", token);
                    user_args.oopts_num_mounts++;
                    token = strtok(NULL, " ");
                    
                }
                break;
            case 'i':
                if (user_args.oopts_exclude_path_regex){
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used with -e option at the same time.\n", opt);
                    exit(EXIT_FAILURE);
                }
                if (user_args.oopts_include_path_regex){
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                }
                sprintf(user_args.oopts_include_path_pattern, "%s", optarg);
                user_args.oopts_include_path_regex = (regex_t *)malloc(sizeof(regex_t));
                if (user_args.oopts_include_path_regex == NULL) {
                    log_message(ERROR, 1, __func__, "-%c option: Could not allocate %u bytes to include_path_regex: %s\n", opt, sizeof(regex_t), optarg);
                    exit(EXIT_FAILURE);
                }
                if (regcomp(user_args.oopts_include_path_regex, optarg, REG_EXTENDED)) {
                    log_message(ERROR, 1, __func__, "-%c option: Could not compile regex for included path: %s\n", opt, optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'e':
                if (user_args.oopts_include_path_regex){
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used with -i option at the same time.\n", opt);
                    exit(EXIT_FAILURE);
                }
                if (user_args.oopts_exclude_path_regex){
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                }
                sprintf(user_args.oopts_exclude_path_pattern, "%s", optarg);
                user_args.oopts_exclude_path_regex = (regex_t *)malloc(sizeof(regex_t));
                if (user_args.oopts_exclude_path_regex == NULL) {
                    log_message(ERROR, 1, __func__, "-%c option: Could not allocate %u bytes to exclude_path_regex: %s\n", opt, sizeof(regex_t), optarg);
                    exit(EXIT_FAILURE);
                }
                if (regcomp(user_args.oopts_exclude_path_regex, user_args.oopts_exclude_path_pattern, REG_EXTENDED)) {
                    log_message(ERROR, 1, __func__, "-%c option: Could not compile regex for excluded path: %s\n", opt, optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'o':
                if (user_args.oopts_output) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                }
                user_args.oopts_output = optarg;
                break;
            case 'I':
                token = strtok(optarg, " ");
                if (user_args.oopts_exclude_pids[0] != 0) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used with -E option at the same time.\n", opt);
                    exit(EXIT_FAILURE);
                }             
                if (user_args.oopts_include_pids[0] != 0) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used more than once.\n", opt);
                    exit(EXIT_FAILURE);
                } 
                for (int i = 0; i < MAX_PROCESS_FILTER; i++) {
                    if (token == NULL) break;
                    if (!is_valid_integer(token)) {
                        log_message(ERROR, 1, __func__, "-%c option: '%s' is not an integer.\n", opt, token);
                        exit(EXIT_FAILURE);
                    } 
                    user_args.oopts_include_pids[i] = atoi(token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'E':
                token = strtok(optarg, " ");
                if (user_args.oopts_include_pids[0] != 0) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used with -I option at the same time.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                if (user_args.oopts_exclude_pids[0] != 0) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used more than once.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                for (int i = 0; i < MAX_PROCESS_FILTER; i++) {
                    if (token == NULL) break;
                    if (!is_valid_integer(token)) {
                        log_message(ERROR, 1, __func__, "%c option: '%s' is not an integer.\n", opt, token);
                        exit(EXIT_FAILURE);
                    } 
                    user_args.oopts_exclude_pids[i] = atoi(token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'N':
                token = strtok(optarg, " ");
                if (user_args.oopts_exclude_process[0][0]) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used with -X option at the same time.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                if (user_args.oopts_include_process[0][0]) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used more than once.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                for (int i = 0; i < MAX_PROCESS_FILTER; i++){
                    if (token == NULL) break;
                    snprintf(user_args.oopts_include_process[i], MAX_PROCESS_NAME_LEN, "%s", token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'X':
                token = strtok(optarg, " ");
                if (user_args.oopts_include_process[0][0]) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used with -N option at the same time.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                if (user_args.oopts_exclude_process[0][0]) {
                    log_message(ERROR, 1, __func__, "-%c option: Cannot be used more than once.\n", opt, token);
                    exit(EXIT_FAILURE);
                } 
                for (int i = 0; i < MAX_PROCESS_FILTER; i++){
                    if (token == NULL) break;
                    snprintf(user_args.oopts_exclude_process[i], MAX_PROCESS_NAME_LEN, "%s", token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'P':
                user_args.oopts_enable_perms_check = true;
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }
    return user_args;
}

/**
 * @brief Prints the usage of the program
 * 
 */
void usage(void){
    printf("Usage: filemon DIRECTORY [-h] [-v] [-o OUTPUT]\n" 
    "%15s[-i INCLUDE_PATERN | -e EXCLUDE_PATTERN]\n"
    "%15s[-I INCLUDE_PIDS | -E EXCLUDE_PIDS]\n"
    "%15s[-N INCLUDE_PROCESS | -X EXCLUDE_PROCESS] [-P]\n", "", "", "");
    printf("Options:\n");
    printf("  %-30s %s\n", "-h  | --help", "Show help");
    printf("  %-30s %s\n", "-v  | --verbose", "Enables debug logs.");
    printf("  %-30s %s\n", "-i  | --include-pattern", "Only show events when path matches regex pattern.");
    printf("  %-30s %s\n", "-e  | --exclude-pattern", "Ignore events when path matches regex pattern.");
    printf("  %-30s %s\n", "-o  | --output", "Output to file");
    printf("  %-30s %s\n", "-I  | --include-pids", "Only show events related to these pids. (Eg. -I \"4728 4279\")");
    printf("  %-30s %s\n", "-E  | --exclude-pids", "Ignore events related to these pids. (Eg. -E \"6728 6817\")");
    printf("  %-30s %s\n", "-N  | --include-process", "Only show events related to these process names. (Eg. -N \"python3 systemd\")");
    printf("  %-30s %s\n", "-X  | --exclude-process", "Ignore events related to these process names. (Eg. -X \"python3 systemd\")");
    printf("  %-30s %s\n", "-P  | --enable-perm-flags", "Add permission flags to fanotify marking. (WARNING: Will slow down system!)");
    return;
} 

#endif
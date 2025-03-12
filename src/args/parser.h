#ifndef ARGS_PARSER_H
#define ARGS_PARSER_H

#include <getopt.h>
#include <string.h>
#include <regex.h>
#include <stdint.h>

#include "../helpers/common.h"
#include "../helpers/mount.h"
#include "../helpers/fanotify.h"

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

#endif
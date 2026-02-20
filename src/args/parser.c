#include "parser.h"

char *allowed_events[MAX_EVENT_FILTERS] = { "read", "write", "execute", "create", "delete", "move" };

/**
 * @brief Parses command-line arguments and returns a structured representation.
 * 
 * This function processes the command-line arguments passed to the program,
 * extracting relevant options and flags to populate a `user_args_t` structure.
 * It validates input parameters and ensures proper usage.
 * 
 * @param argc The number of command-line arguments.
 * @param argv An array of argument strings.
 * @return user_args_t A structure containing parsed argument values.
 *                     If an error occurs, appropriate error handling should be performed.
 */
user_args_t parse_args(int argc, char *argv[]) {

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
        .oopts_enable_perms_check =  false,
        .oopts_events = {NULL}
    };    

    struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
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
        {"events", required_argument, 0, 's'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hvVm:i:e:o:I:E:N:X:Ps:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                user_args.oopts_verbose = 2; 
                break;
            case 'V':
                printf("filemon %s\n", FILEMON_VERSION_STR);
                exit(EXIT_SUCCESS);
                break;
            case 'm':
                if (optarg[0] == '\0') {
                    log_message(ERROR, __func__, "-%c option: No mount points were specified! Please state at least one!", opt);
                    exit(EXIT_FAILURE);
                }

                char **all_mounts = get_all_mount_points(&total_num_mounts);
                if (!all_mounts) {
                    log_message(ERROR, __func__, "-%c option: Failed to get relevant mount points!", opt);
                    exit(EXIT_FAILURE);
                }
                user_args.oopts_num_mounts = 0;
                token = strtok(optarg, " ");
                for (size_t i = 0; i < MAX_MOUNT_POINTS; i++) {
                    if (token == NULL) 
                        break;
                    if (!is_mount_point(token, all_mounts, total_num_mounts)) {
                        log_message(ERROR, __func__, "-%c option: \"%s\" is not a mount point!", opt, token);
                        exit(EXIT_FAILURE);
                    }
                    snprintf(user_args.oopts_mounts[i], PATH_MAX, "%s", token);
                    user_args.oopts_num_mounts++;
                    token = strtok(NULL, " ");
                }
                free_mount_points(all_mounts, total_num_mounts);
                break;
            case 'i':
                if (user_args.oopts_exclude_path_regex){
                    log_message(ERROR, __func__, "-%c option: Cannot be used with -e option at the same time.", opt);
                    exit(EXIT_FAILURE);
                }
                if (user_args.oopts_include_path_regex){
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                }
                sprintf(user_args.oopts_include_path_pattern, "%s", optarg);
                user_args.oopts_include_path_regex = (regex_t *)malloc(sizeof(regex_t));
                if (user_args.oopts_include_path_regex == NULL) {
                    log_message(ERROR, __func__, "-%c option: Could not allocate %u bytes to include_path_regex: %s", opt, sizeof(regex_t), optarg);
                    exit(EXIT_FAILURE);
                }
                if (regcomp(user_args.oopts_include_path_regex, optarg, REG_EXTENDED)) {
                    log_message(ERROR, __func__, "-%c option: Could not compile regex for included path: %s", opt, optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'e':
                if (user_args.oopts_include_path_regex){
                    log_message(ERROR, __func__, "-%c option: Cannot be used with -i option at the same time.", opt);
                    exit(EXIT_FAILURE);
                }
                if (user_args.oopts_exclude_path_regex){
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                }
                sprintf(user_args.oopts_exclude_path_pattern, "%s", optarg);
                user_args.oopts_exclude_path_regex = (regex_t *)malloc(sizeof(regex_t));
                if (user_args.oopts_exclude_path_regex == NULL) {
                    log_message(ERROR, __func__, "-%c option: Could not allocate %u bytes to exclude_path_regex: %s", opt, sizeof(regex_t), optarg);
                    exit(EXIT_FAILURE);
                }
                if (regcomp(user_args.oopts_exclude_path_regex, user_args.oopts_exclude_path_pattern, REG_EXTENDED)) {
                    log_message(ERROR, __func__, "-%c option: Could not compile regex for excluded path: %s", opt, optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'o':
                if (user_args.oopts_output) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                }
                user_args.oopts_output = optarg;
                break;
            case 'I':
                token = strtok(optarg, " ");
                if (user_args.oopts_exclude_pids[0] != 0) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used with -E option at the same time.", opt);
                    exit(EXIT_FAILURE);
                }             
                if (user_args.oopts_include_pids[0] != 0) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                } 
                for (size_t i = 0; i < MAX_PROCESS_FILTER; i++) {
                    if (token == NULL) break;
                    if (!is_valid_integer(token)) {
                        log_message(ERROR, __func__, "-%c option: '%s' is not an integer.", opt, token);
                        exit(EXIT_FAILURE);
                    } 
                    user_args.oopts_include_pids[i] = atoi(token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'E':
                token = strtok(optarg, " ");
                if (user_args.oopts_include_pids[0] != 0) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used with -I option at the same time.", opt);
                    exit(EXIT_FAILURE);
                } 
                if (user_args.oopts_exclude_pids[0] != 0) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                } 
                for (size_t i = 0; i < MAX_PROCESS_FILTER; i++) {
                    if (token == NULL) break;
                    if (!is_valid_integer(token)) {
                        log_message(ERROR, __func__, "%c option: '%s' is not an integer.", opt, token);
                        exit(EXIT_FAILURE);
                    } 
                    user_args.oopts_exclude_pids[i] = atoi(token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'N':
                token = strtok(optarg, " ");
                if (user_args.oopts_exclude_process[0][0]) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used with -X option at the same time.", opt);
                    exit(EXIT_FAILURE);
                } 
                if (user_args.oopts_include_process[0][0]) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                } 
                for (size_t i = 0; i < MAX_PROCESS_FILTER; i++){
                    if (token == NULL) break;
                    snprintf(user_args.oopts_include_process[i], MAX_PROCESS_NAME_LEN, "%s", token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'X':
                token = strtok(optarg, " ");
                if (user_args.oopts_include_process[0][0]) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used with -N option at the same time.", opt);
                    exit(EXIT_FAILURE);
                } 
                if (user_args.oopts_exclude_process[0][0]) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                } 
                for (size_t i = 0; i < MAX_PROCESS_FILTER; i++){
                    if (token == NULL) break;
                    snprintf(user_args.oopts_exclude_process[i], MAX_PROCESS_NAME_LEN, "%s", token);
                    token = strtok(NULL, " ");
                }
                break;
            case 'P':
                if (!has_config_fanotify_access_perms()) {
                    log_message(ERROR, __func__, "Unable to enable permission checks as kernel does not support this feature!");
                    exit(EXIT_FAILURE);
                }
                user_args.oopts_enable_perms_check = true;
                break;
            case 's':
                token = strtok(optarg, " ");
                if (user_args.oopts_events[0] != NULL) {
                    log_message(ERROR, __func__, "-%c option: Cannot be used more than once.", opt);
                    exit(EXIT_FAILURE);
                } 
                for (size_t i = 0; i < MAX_EVENT_FILTERS; i++){
                    if (token == NULL) break;
                    if (!is_valid_event(token)) {
                        log_message(ERROR, __func__, "-%c option: Not a valid event! Allowed events: [\"read\", \"write\", \"execute\", \"create\", \"delete\", \"move\"]", opt);
                        exit(EXIT_FAILURE);
                    }
                    user_args.oopts_events[i] = strdup(token);
                    token = strtok(NULL, " ");
                }
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        log_message(ERROR, __func__, "filemon does not take in any positional arguments! See usage.");
        usage();
        exit(EXIT_FAILURE);
    }
    return user_args;
}

/**
 * @brief Prints the usage of filemon.
 * 
 */
void usage(void){
    printf("Usage: filemon DIRECTORY [-h] [-v] [-V] [-m MOUNTS] [-o OUTPUT]\n" 
    "%15s[-i INCLUDE_PATH_PATTERN | -e EXCLUDE_PATH_PATTERN]\n"
    "%15s[-I INCLUDE_PIDS | -E EXCLUDE_PIDS]\n"
    "%15s[-N INCLUDE_PROCESS | -X EXCLUDE_PROCESS] [-P] [-s EVENTS]\n", "", "", "");
    printf("Options:\n");
    printf("  %-30s %s\n", "-h  | --help", "Show help");
    printf("  %-30s %s\n", "-v  | --verbose", "Enables debug logs.");
    printf("  %-30s %s\n", "-V  | --version", "Show the version of filemon.");
    printf("  %-30s %s\n", "-m  | --mounts", "Mounts to monitor. Default value: \"/\" (Eg. -m \"/tmp /opt /\")");
    printf("  %-30s %s\n", "", "Maximum number of 5 mounts. Last stated mount has highest priority.");
    printf("  %-30s %s\n", "-i  | --include-path-pattern", "Only show events when path matches regex pattern.");
    printf("  %-30s %s\n", "-e  | --exclude-path-pattern", "Ignore events when path matches regex pattern.");
    printf("  %-30s %s\n", "-o  | --output", "Output to file. (\".txt\", \".csv\", \".json\", \".jsonl\")");
    printf("  %-30s %s\n", "-I  | --include-pids", "Only show events related to these pids. (Eg. -I \"4728 4279\")");
    printf("  %-30s %s\n", "-E  | --exclude-pids", "Ignore events related to these pids. (Eg. -E \"6728 6817\")");
    printf("  %-30s %s\n", "-N  | --include-process", "Only show events related to these process names. (Eg. -N \"python3 systemd\")");
    printf("  %-30s %s\n", "-X  | --exclude-process", "Ignore events related to these process names. (Eg. -X \"python3 systemd\")");
    printf("  %-30s %s\n", "-P  | --enable-perm-flags", "Add permission flags to fanotify marking. (WARNING: Will slow down system!)");
    printf("  %-30s %s\n", "-s  | --events", "Filter by events (Eg. -s \"write create\")");
    printf("  %-30s %s\n", "", "Maximum number of 6 events: [\"read\", \"write\", \"execute\", \"create\", \"delete\", \"move\"]");
    return;
} 

/**
 * @brief Checks if the given string represents a valid integer.
 * 
 * This function attempts to convert the input string to a long integer using
 * `strtol`. It ensures the string is a valid integer representation by checking
 * for errors such as out-of-range values, invalid characters, or empty strings.
 * 
 * @param str The string to check.
 * @return `true` if the string is a valid integer, `false` otherwise.
 * 
 * @note The function uses `strtol` to convert the string and checks for errors
 * like out-of-range values (`LONG_MAX`, `LONG_MIN`) and invalid characters.
 * It also ensures the string doesn't contain extraneous non-numeric characters.
 */
bool is_valid_integer(const char *str) {
    char *endptr;
    errno = 0;

    long val = strtol(str, &endptr, 10);

    if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) {
        return false;
    }

    if (errno != 0 && val == 0) {
        return false;
    }

    if (endptr == str) {
        return false;
    }

    if (*endptr != '\0') {
        return false;
    }

    return true;
}

bool is_valid_event(const char *event) {
    for (size_t i = 0; i < MAX_EVENT_FILTERS; i++) {
        if (strcmp(event, allowed_events[i]) == 0)
            return true;
    }
    return false;
}
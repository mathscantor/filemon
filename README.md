# **Filemon** - Linux File Monitoring using Fanotify



## Description

Filemon is created specifically for Linux Operating Systems in monitoring files and directories accessed by different processes. I have written this tool as a way to showcase the usage of all `FAN_*` flags. The monitoring of file/directory creation, deletion, renaming etc. is handled by one thread while the monitoring of read, write, execute etc. events are handled by another thread.

All events are properly logged with a timestamp, process name, PID, file/directory path and its `FAN_*` flags.

## Compilation

To compile filemon, simply run the following command:

```bash
$ make clean all
```

A successfully built Filemon will be generated in **build/filemon**.

## Usage

As fanotify requires root permissions, remember to run it with sudo or change to the root user before running!

```
Usage: filemon [-h] [-v] [-o OUTPUT] [-m MOUNT]
               [-i INCLUDE_PATERN | -e EXCLUDE_PATTERN]
               [-I INCLUDE_PIDS | -E EXCLUDE_PIDS]
               [-N INCLUDE_PROCESS | -X EXCLUDE_PROCESS] DIRECTORY
Options:
  -h  | --help                   Show help
  -v  | --verbose                Enables debug logs.
  -i  | --include-pattern        Only show events when path matches regex pattern.
  -e  | --exclude-pattern        Ignore events when path matches regex pattern.
  -o  | --output                 Output to file
  -m  | --mount                  The mount path. (Use this option to override auto search from fstab)
  -I  | --include-pids           Only show events related to these pids. (Eg. -I "4728 4279")
  -E  | --exclude-pids           Ignore events related to these pids. (Eg. -E "6728 6817")
  -N  | --include-process        Only show events related to these process names. (Eg. -N "python3 systemd")
  -X  | --exclude-process        Ignore events related to these process names. (Eg. -X "python3 systemd")
```

### Example 1 - Simple Usage

Simply state the directory path to monitor. This will recursively monitor all sub-directories, including the parent directory as well.

```
# ./build/filemon /tmp/new

16-08-2024 23:34:36.370 UTC+08:00    [INF] Starting filemon...
16-08-2024 23:34:36.373 UTC+08:00    [INF] Monitor Box Information:
============================ MONITOR BOX ===========================
- Parent Path: /tmp/new
- Mount Path: /

------------------- FANOTIFY INFO -------------------
- CONFIG_FANOTIFY Enabled: 1
- CONFIG_FANOTIFY_ACCESS_PERMISSIONS Enabled: 1
- Fanotify Read, Write, Execute FD: 3
        └─ Flags: FAN_ACCESS, FAN_OPEN, FAN_MODIFY, FAN_OPEN_EXEC, FAN_CLOSE_WRITE, FAN_CLOSE_NOWRITE, FAN_OPEN_PERM, FAN_ACCESS_PERM, FAN_OPEN_EXEC_PERM
- Fanotify Create, Delete, Move FD: 4
        └─ Flags: FAN_CREATE, FAN_DELETE, FAN_RENAME, FAN_MOVED_FROM, FAN_MOVED_TO

---------------------- FILTERS ----------------------
- Include PIDs: 
- Exclude PIDs: 

- Include Processes: 
- Exclude Processes: 

- Include Pattern: 
- Exclude Pattern:

=====================================================================
16-08-2024 23:34:36.373 UTC+08:00    [INF] Successfully started filemon.
16-08-2024 23:34:50.718 UTC+08:00    [INF] mkdir (6933): /tmp/new/aa == [FAN_CREATE, FAN_ONDIR]
16-08-2024 23:35:06.195 UTC+08:00    [INF] touch (7006): /tmp/new/aa/testfile == [FAN_OPEN_PERM]
16-08-2024 23:35:06.195 UTC+08:00    [INF] touch (7006): /tmp/new/aa/testfile == [FAN_CREATE]
16-08-2024 23:35:06.195 UTC+08:00    [INF] touch (7006): /tmp/new/aa/testfile == [FAN_OPEN]
16-08-2024 23:35:06.195 UTC+08:00    [INF] touch (7006): /tmp/new/aa/testfile == [FAN_CLOSE_WRITE]
...
```

### Example 2 - Ignore Events From Certain Path Using Regex

This option is great when you are dealing with a directory that contains many files and you specifically know what kind of files/sub-directories to ignore.

```
# ./build/filemon -e ".*\.txt" /tmp/new

16-08-2024 23:38:10.979 UTC+08:00    [INF] Starting filemon...
...
16-08-2024 23:38:10.981 UTC+08:00    [INF] Successfully started filemon.
16-08-2024 23:38:51.800 UTC+08:00    [INF] rm (7691): /tmp/new/aa == [FAN_DELETE, FAN_ONDIR]
...
```

### Example 3 - Show Events by PIDs

You can also filter for events that match a given list of PIDs.

```
# ./build/filemon -I "22467 22498" /tmp/new

16-08-2024 23:42:21.469 UTC+08:00    [INF] Starting filemon...
...
04-09-2024 16:26:38.869 UTC+08:00    [INF] Successfully started filemon.
04-09-2024 16:27:14.335 UTC+08:00    [INF] python3 (22467): /tmp/new/test == [FAN_OPEN_PERM]
04-09-2024 16:27:14.335 UTC+08:00    [INF] python3 (22467): /tmp/new/test == [FAN_OPEN]
04-09-2024 16:27:14.335 UTC+08:00    [INF] python3 (22467): /tmp/new/test == [FAN_CREATE]
04-09-2024 16:27:18.142 UTC+08:00    [INF] python3 (22467): /tmp/new/test == [FAN_CLOSE_WRITE]
04-09-2024 16:27:30.015 UTC+08:00    [INF] python3 (22498): /tmp/new/test == [FAN_DELETE]
...
```

### Example 4 - Log All Outputs to File

To redirect all outputs for `stdout` to a file, use the `-l` option and specify a file path to store the logs in.

```
# ./build/filemon -o log.txt /tmp/new

[+] Starting filemon...
[+] Successfully started filemon.
[+] All output is redirected to "/home/user/repositories/filemon/log.txt"
```


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
# ./build/filemon -h

Usage: filemon [-h|--help] [-v] [-e PATTERN] [-l LOGFILE] DIRECTORY
-h  | --help                   Show help
-v  | --verbose                Enables debug logs.
-e  | --exclude-pattern        Ignore events when path matches regex pattern.
-l  | --log                    The path of the log file.
```



### Example 1 - Simple Usage

Simply state the directory path to monitor. This will recursively monitor all sub-directories the parent directory as well.

```
# ./build/filemon /tmp/new

16-08-2024 23:34:36.370 UTC+08:00    [INF] Starting filemon...
16-08-2024 23:34:36.373 UTC+08:00    [INF] filemon has started successfully.
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
16-08-2024 23:38:10.981 UTC+08:00    [INF] filemon has started successfully.
16-08-2024 23:38:51.800 UTC+08:00    [INF] rm (7691): /tmp/new/aa == [FAN_DELETE, FAN_ONDIR]
...
```



### Example 3 - Verbose Output

To look at the debug messages, use the `-v` flag to increase the verbosity of the logs.

```
# ./build/filemon -v /tmp/new

16-08-2024 23:42:21.469 UTC+08:00    [INF] Starting filemon...
16-08-2024 23:42:21.471 UTC+08:00    [DBG] Parent Path: /tmp/new
16-08-2024 23:42:21.471 UTC+08:00    [DBG] exclude_pattern: 
16-08-2024 23:42:21.471 UTC+08:00    [DBG] fan_fd_read_write_execute: 5
16-08-2024 23:42:21.471 UTC+08:00    [DBG] fan_fd_create_delete_move: 6
16-08-2024 23:42:21.471 UTC+08:00    [DBG] FAN_MARK_ADD: /tmp/new
16-08-2024 23:42:21.471 UTC+08:00    [INF] filemon has started successfully.
16-08-2024 23:42:37.177 UTC+08:00    [DBG] FAN_MARK_ADD: /tmp/new/aa
16-08-2024 23:42:37.177 UTC+08:00    [INF] mkdir (7995): /tmp/new/aa == [FAN_CREATE, FAN_ONDIR]
16-08-2024 23:42:58.284 UTC+08:00    [INF] rmdir (8070): /tmp/new/aa == [FAN_DELETE, FAN_ONDIR]
...
```



### Example 4 - Log All Outputs to File

To redirect all outputs for `stdout` to a file, use the `-l` option and specify a file path to store the logs in.

```
# ./build/filemon -l log.txt /tmp/new

[+] Starting filemon...
[+] filemon has started successfully.
[+] All output is redirected to 'log.txt'
```


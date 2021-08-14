#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "commands.h"

#define MAX_PROCESSES 500       // max number of processes allowed

int foreground_mode;                // regular mode == 0, foreground only mode == 1


int main(int argc, char* argv[]) {    
    pid_t shell_pid = getpid();             // shell pid
    int process_status;                     // process status
    int num_processes_running = 0;          // number of processes running
    pid_t processes[MAX_PROCESSES];         // list of running process ids


    while(1) {
        // prompt user, parse input, and initialize a Command struct
        Command cmd = get_command(shell_pid);

        // command given is a built-in one
        if (built_in_command(&cmd)) {
            run_built_in(&cmd, &process_status, num_processes_running, processes);
        }

        // command given is not built-in
        else {
            run_external_command(&cmd, &process_status, processes, &num_processes_running, &foreground_mode);
        }
    }
    return 0;
}
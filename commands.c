#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include "commands.h"

#define MAX_INPUT_CHARS 2048        // maximum characters one can input in one line
#define MAX_INPUT_BUF_SIZE 2050     // 
#define MAX_ARGS 512

int* copy_fg_mode; 

int count_vars_to_expand(char* argument) {
    // Count the number of times '$$' occurs in an argument
    int num_vars = 0;
    int i;

    for (i = 0; i < strlen(argument); i++){
        // only count occurences of '$$'
        if (argument[i] == '$' && (argument[i+1] && argument[i+1] == '$')) {
            num_vars++;
            i++;
        }
    }
    return num_vars;
}

char* pid_to_string(pid_t pid) {
    // Convert the process ID to a string
    char* pid_buffer;         
    int pid_buf_size = (int)(ceil(log10(pid))+1);           // calculates number of digits in pid, source: https://stackoverflow.com/a/8257728
    pid_buffer = malloc(pid_buf_size);

    sprintf(pid_buffer, "%d", pid);                         // send pid as string to buffer
    
    return pid_buffer;
}

char* expand_variable(char* argument, int num_vars, pid_t pid) {
    // Variable expansion of '$$' to process ID
    // Returns new argument

    char* pid_string = pid_to_string(pid);            // pid as a string
    
    // argument with expanded variables
    char* new_arg;
    int new_arg_size = ((strlen(pid_string) * num_vars) + ((strlen(argument)+1) - (num_vars * 2)));         // new length of argument
    new_arg = calloc(new_arg_size, sizeof(char));
    
    int i;
    int j = 0;

    // build new argument string
    for (i = 0; i < strlen(argument); i++) {
        // variable to expand
        if (argument[i] == '$' && (argument[i+1] && argument[i+1] == '$')) {
            strcat(new_arg, pid_string);    // concatenate pid to argument
            j += strlen(pid_string);
            i++;
        }
        // normal letter
        else {
            new_arg[j++] = argument[i];     // add letter
        }
    }

    return new_arg;
}

int tokenize_input(char commandInput[], char* arguments[], pid_t shell_pid) {
    // Tokenizes input by spaces into separate arguments
    // Returns number of tokens

    char* saveptr;              // tracks position in string
    int token_count = 0;        
    char* token = strtok_r(commandInput, " ", &saveptr);
    
    int num_vars_to_expand;     // number of variables to expand per token

    while(token != NULL) {
        // expand all variables found in token
        num_vars_to_expand = count_vars_to_expand(token);
        if (num_vars_to_expand) {
            token = expand_variable(token, num_vars_to_expand, shell_pid);
        }

        // add token to list of arguments
        arguments[token_count] = strdup(token);
        token_count++;
        token = strtok_r(NULL, " ", &saveptr);
    }

    return token_count;
}

int check_input_validity(char* arguments[], int arg_count) {
    // Checks if the arguments provided come before any file redirection
    // Returns 1 if they do, otherwise returns 0

    int i; 

    for (i = 0; i < arg_count; i++) {
        // midline comment encountered
        if (strcmp(arguments[i], "//") == 0) {
            return 0;
        }
        // check when a redirect is encountered
        if ((strcmp(arguments[i], "<") == 0) || (strcmp(arguments[i], ">")) == 0) {
            // check if there's another argument after the filename
            if (arguments[i+2]) {
                // next argument is another redirect or an '&' symbol at the end of the arguments
                if ((strcmp(arguments[i+2], "<") == 0) || (strcmp(arguments[i+2], ">") == 0) || (strcmp(arguments[i+2], "&") == 0 && (i+2 == arg_count - 1))) {
                    return 1;
                }
                // next argument is invalid
                else {
                    return 0;
                }
            }
            return 1;
        }
    }
    return 1;
}

void init_command(Command* cmd, char* arguments[], int arg_count) {
    /*
    Initializes the Command struct with the arguments provided.
    */
   
    // initialize all struct data members to null
    int i;
    for (i = 0; i < MAX_ARGS; i++) {
        cmd->args[i] = NULL;
    }
    cmd->command = NULL;
    cmd->input_file = NULL;         
    cmd->output_file = NULL;
    cmd->is_bg = 0;
    
    int j = 0;
    // flags for readability
    int redirecting_in;
    int redirecting_out;
    int run_in_bg;

    // go through provided arguments and initialize Command struct
    for (i = 0; i < arg_count; i++) {
        redirecting_in = strcmp(arguments[i], "<") == 0 ? 1 : 0;        // redirect in encountered: 1 == True, 0 == False
        redirecting_out = strcmp(arguments[i], ">") == 0 ? 1 : 0;       // redirect out encountered: 1 == True, 0 == False
        run_in_bg = strcmp(arguments[i], "&") == 0 ? 1 : 0;             // bg operator encountered: 1 == True, 0 == False

        // first argument is the command
        if (i == 0) {
            cmd->command = strdup(arguments[i]);
        }
        // input file provided
        if (redirecting_in) {
            cmd->input_file = strdup(arguments[i+1]);
            i++;
        }
        // output file provided
        else if (redirecting_out) {
            cmd->output_file = strdup(arguments[i+1]);
            i++;
        }
        // run in background provided
        else if (run_in_bg) {
            cmd->is_bg = 1;
        }
        // regular argument
        else {
            cmd->args[j] = strdup(arguments[i]);
            j++;
        }
    }
}

Command get_command(pid_t shell_pid) {
    /*
    Prompts the user for commands and initializes a Command struct.
    Returns: Command struct.
    */

    // command struct
    Command cmd;

    // arguments array
    char* arguments[512];
    
    // input buffer
    char* input = NULL;
    size_t input_len = 0;
    
    // flags for readability
    int num_chars;
    int input_exceeds_max_chars;
    int blank_line_provided;
    int comment_line_provided;
    int valid_input;
    int num_tokens;

    //Prompt user and read input until a valid input is provided
    do {
        printf(": ");
        fflush(stdout);
        getline(&input, &input_len, stdin);

        // number of characters
        num_chars = strlen(input) - 1;

        // replace newline character from input
        input[num_chars] = '\0';

        // initialize flags
        input_exceeds_max_chars = num_chars > MAX_INPUT_CHARS;
        blank_line_provided = num_chars < 1;
        comment_line_provided = input[0] == '#';

        // Display message when exceeding allowable characters
        if (input_exceeds_max_chars) {
            printf("You entered %d characters which exceeds the maximum of 2048 characters. Please try again.\n", num_chars);
            fflush(stdout);
        }

        // Don't tokenize the input if blank, is a comment, or exceeds max chars
        if (blank_line_provided || comment_line_provided || input_exceeds_max_chars) {
            valid_input = 0;
        }
        // Tokenize input and check if valid
        else {
            num_tokens = tokenize_input(input, arguments, shell_pid);
            valid_input = check_input_validity(arguments, num_tokens);
        }
    } while (!valid_input);

    // input validated, initialize Command struct
    init_command(&cmd, arguments, num_tokens);
    
    free(input);
    return cmd;
}

char *safe(char *s) {
    // Safely print null strings
    return s ? s : "NULL";
}

void display_command(Command* cmd) {
    printf("command: %s\n", safe(cmd->command));
    fflush(stdout);

    int i = 0;
    char* arg;
    while ((arg = cmd->args[i]) != NULL) {
        printf("arg[%d]: %s\n", i, safe(arg));
        fflush(stdout);
        i++;
    }

    printf("input file: %s\n", safe(cmd->input_file));
    fflush(stdout);
    printf("output file: %s\n", safe(cmd->output_file));
    fflush(stdout);
    printf("is_bg: %d\n", cmd->is_bg);
    fflush(stdout);
};

int built_in_command(Command* cmd) {
    /*
    Checks if the command provided is a built-in command
    Returns 1 if built-in, otherwise return 0
    */

    char* built_ins[] = {"exit", "cd", "status"};           // list of built-in commands available
    int num_built_ins = sizeof(built_ins) / sizeof(built_ins[0]);
    int i;

    // iterate list of allowable built-ins
    for (i = 0; i < num_built_ins; i++) {
        // command is built-in
        if (strcmp(cmd->command, built_ins[i]) == 0) {
            return 1;
        }
    }
    // command given is not built-in
    return 0;
}

void exit_cmd(int num_processes, pid_t processes[]) {
    /*
    Built-in exit command, exits the shell.
    Kills all processes that have been started before terminating. 
    */

    // no processes have been started
    if (num_processes == 0) {
        exit(0);
    }
    // processes have been started, terminate all of background processes before exiting
    else {
        int status;
        int i;
        for (i = 0; i < num_processes; i++) {
            kill(processes[i], SIGTERM);                // terminate process
            waitpid(processes[i], &status, WNOHANG);    // remove zombie process
        }
        exit(0);
    }
}

void status_cmd(int status) {
    /*
    Prints out the exit status or the terminating signal of the last foreground process ran by the shell.
    Returns exit status of 0 if called before any non-built-in foreground command is run.
    */
    
    // process was terminated normally, print status value
    if (WIFEXITED(status)) {
        printf("exit value %d\n", WEXITSTATUS(status));
        fflush(stdout);
    }
    // process was terminated abnormally, print signal number that caused the termination
    else {
        printf("terminated by signal %d\n", WTERMSIG(status));
        fflush(stdout);
    }
}

void cd_cmd(Command* cmd) {
    /*
    Changes current directory to the directory provided, if none provided it changes to the home directory.
    */
    int directory_provided = cmd->args[1] ? 1 : 0;
    
    // directory name provided
    if (directory_provided) {
        // directory path is invalid, display error message
        if (chdir(cmd->args[1]) != 0) {
            printf("%s - no such file or directory.\n", cmd->args[1]);
            fflush(stdout);
        }
        // directory path is valid, change to the directory
        else {
            chdir(cmd->args[1]);
        }
    }
    // no directory name provided, change to home directory
    else {
        chdir(getenv("HOME"));
    }
}

void run_built_in(Command* cmd, int* status, int num_processes_running, pid_t processes[]) {
    // command given: exit
    if (strcmp(cmd->command, "exit") == 0) {
        exit_cmd(num_processes_running, processes);
    }
    // command given: cd
    else if (strcmp(cmd->command, "cd") == 0) {
        cd_cmd(cmd);
    }
    // command given: status
    else {
        // no commands have been ran yet
        status_cmd(*status);
    }
}

void remove_pid_from_processes(pid_t pid, int processes[], int* num_processes) {
    int i;
    int index_removed = 0;
    for (i = 0; i < *num_processes; i++) {
        if (processes[i] == pid) {
            index_removed = i;
        }  
    }

    processes[index_removed] = processes[*num_processes - 1];
    *num_processes -= 1;
}

pid_t check_background_processes() {
    pid_t pid;          // pid of terminated child
    int status;         // termination status

    // check if any background child process has terminated
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("background pid %d is done: ", pid);

        // process was terminated normally, print status value
        if (WIFEXITED(status)) {
            printf("exit value %d\n", WEXITSTATUS(status));
        }
        // process was terminated abnormally, print signal number causing termination
        else {
            printf("terminated by signal %d\n", WTERMSIG(status));
        }
        
        fflush(stdout);
        return pid;
    }

    return 0;
}

void open_and_redirect(int* fd, char* filename, int* status, int open_type) {
    /*
    Opens a file and redirects stdin or stdout according to open_type value.
    If open_type == 0, stdin is redirected to the file.
    If open_type == 1, stdout is redirected to the file.
    */
    
    char* type = open_type == 0 ? "input" : "output";
    
    // open the file for reading
    if (open_type == 0) {
        *fd = open(filename, O_RDONLY);
    }
    // open the file for writing
    else {
        *fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }

    // error openning file
    if (*fd == -1) {
        printf("cannot open %s for %s\n", filename, type);
        fflush(stdout);
        *status = 1;
        exit(1);
    }
    // file openned
    else {
        // redirect stdin or stdout to file
        int redirect_status = dup2(*fd, open_type);

        // error redirecting stdin or stdout to file
        if (redirect_status == -1) {
            printf("unable to redirect %s to %s\n", filename, type);
            fflush(stdout);
            *status = 1;
            exit(1);
        }
    }
}

void handle_SIGTSTP(int signo) {
    // currently in regular mode
    if (*copy_fg_mode == 0) {
        // change to foreground mode
        *copy_fg_mode = 1;
        write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 51);
        fflush(stdout);
    }

    // currently in foreground mode
    else {
        // change to regular mode
        *copy_fg_mode = 0;
        write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 31);
        fflush(stdout);
    }
    write(STDOUT_FILENO, ": ", 3);
    fflush(stdout);
}  

void check_foreground_mode(Command* cmd) {
    // When in foreground mode, update all commands to foreground only
    if (*copy_fg_mode == 1) {
        cmd->is_bg = 0;
    } 
}

void run_external_command(Command* cmd, int* status, pid_t processes[], int* num_processes, int* foreground_mode) {
    /*
    Executes all external commands.
    */

    copy_fg_mode = foreground_mode;                 // global foreground_mode address
    check_foreground_mode(cmd);
    pid_t spawn_pid = -2;
    int child_status;                           // child process status
    int background_process = cmd->is_bg;        // run in background?
    int in_fd;                                  // input file descriptor
    int out_fd;                                 // output file descriptor

    // intialize SIGINT_action struct
    // source: Signal Handling API module
    struct sigaction SIGINT_action = {{0}};
    SIGINT_action.sa_handler = SIG_IGN;              // ignore SIGINT
    sigfillset(&SIGINT_action.sa_mask);              // block all catchable signals 
    sigaction(SIGINT, &SIGINT_action, NULL);        // install signal handler

    // initialize SIGTSTP_action struct
    struct sigaction SIGTSTP_action = {{0}};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;      // handler for SIGTSTP
    SIGTSTP_action.sa_flags = SA_RESTART;           // restart any interrupted signal calls
    sigfillset(&SIGTSTP_action.sa_mask);            // block all catchable signals
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);      // install signal handler


    spawn_pid = fork();                             // fork a process

    // fork failed
    if (spawn_pid == -1) {
        printf("fork() failed!\n");
        fflush(stdout);
        exit(1);
    }

    // child process
    else if (spawn_pid == 0) {
        int reading = 0;
        int writing = 1;
        
        // ignore SIGTSTP signals
        SIGTSTP_action.sa_handler = SIG_IGN;
        sigaction(SIGTSTP, &SIGTSTP_action, NULL);

        // background process
        if (background_process) {
            // input file given, redirect stdin to file
            if (cmd->input_file) {
                open_and_redirect(&in_fd, cmd->input_file, &child_status, reading);
            }
            // input file not given, redirect input to /dev/null
            else {
                open_and_redirect(&in_fd, "/dev/null", &child_status, reading);
            }

            // output file given, redirect stdout to file
            if (cmd->output_file) {
                open_and_redirect(&out_fd, cmd->output_file, &child_status, writing);
            }
            // output file not given, redirect stdout to /dev/null
            else {
                open_and_redirect(&out_fd, "/dev/null", &child_status, writing);
            }
        }

        // foreground processes
        else {
            // allow process to receive SIGINT
            SIGINT_action.sa_handler = SIG_DFL;
            sigaction(SIGINT, &SIGINT_action, NULL);

            // input file given, redirect stdin to file
            if (cmd->input_file) {
                open_and_redirect(&in_fd, cmd->input_file, status, reading);
            }
            // output file given, redirect stdout to file
            if (cmd->output_file) {
                open_and_redirect(&out_fd, cmd->output_file, status, writing);
            }
        }

        close(in_fd);
        close(out_fd);

        // run command
        execvp(cmd->command, cmd->args);

        // command failed since execvp returned
        printf("Command '%s' could not be found.\n", cmd->command);
        fflush(stdout);
        exit(1);
    }
    // parent process
    else {
        // Close input and output files after finished executing command
        // source: Processes and I/O module
        fcntl(in_fd, F_SETFD, FD_CLOEXEC);
        fcntl(out_fd, F_SETFD, FD_CLOEXEC);

        // command ran in background
        if (background_process) {
            waitpid(spawn_pid, &child_status, WNOHANG);     // don't wait for process to terminate, return command line access and control to user
            
            printf("background pid is %d\n", spawn_pid);
            fflush(stdout);

            processes[*num_processes] = spawn_pid;          // save background process child pid
            *num_processes += 1;                            // increment number of background process

        }
        // command ran in foreground
        else {
            // wait for child process to terminate
            waitpid(spawn_pid, &child_status, 0);
        
            // foreground process was terminated abnormally
            if (WIFSIGNALED(child_status)) {
                char* msg = "\nterminated by signal 2\n";
                write(STDOUT_FILENO, msg, 25);
                fflush(stdout);
            }
            // save status of foreground process
            *status = child_status;

        }
    }
  
    pid_t bg_pid;
    // check status of background processes
    bg_pid = check_background_processes();
    // a process has terminated, update list of background pids
    if (bg_pid) {
        remove_pid_from_processes(bg_pid, processes, num_processes);
    }
}
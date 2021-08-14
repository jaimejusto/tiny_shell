#define MAX_ARGS 512        // max number of arguments

typedef struct Commands {
    char* command;          // command
    char* args[MAX_ARGS];   // arguments exclude input and output filenames and bg flag
    char* input_file;       // input filename
    char* output_file;      // output filename
    int is_bg;              // 1 == background process, 0 == foreground process
} Command;

Command get_command(pid_t shell_pid);
int built_in_command(Command* cmd);
void run_built_in(Command* cmd, int* process_status, int num_processes_running, pid_t processes[]);
void display_command(Command* cmd);
void run_external_command(Command* cmd, int* status, pid_t processes[], int* num_processes, int* foreground_mode);
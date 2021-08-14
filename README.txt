Shell


Command line syntax: 

    command [arg1 arg2 ...] [< input_file] [> output_file] [&]
    
    - The command lines have a maximum length of 2048 characters and a maximum of 512 arguments.
    - Items in square brackets are optional.
    - Commands desired to be executed in the background should include '&' at the end.
    - Redirection of standard input or output must appear after all the arguments.
    - Midline comments are not supported. 
    - Any instance of '$$' in a command is expanded into the process ID of the shell itself. 


Shell comes with three built-in commands: exit, cd, and status. These will always be ran in the foreground.
    - exit
        - Exits the shell and takes no arguments. Kills all proceses or jobs that are still ongoing before 
          terminating itself.

    - cd [path]
        - Changes the working directory to the directory specified by path. 
        - path can be absolute or relative.
        - If path not provided, it changes the working directory to the directory specified in the HOME 
          environment variable.

    - status
        - Prints out either the exit status or the terminating signal of the last foreground process ran by 
          the shell.
        - Returns the exit status 0 if ran before any foreground command is run.
        - The three built-in shell commands don't count as foreground processes for this command. 


All other commands are executed as new processes. If the shell couldn't find the command to run, then the 
shell will print an error message and set the exit status to 1. 


The shell waits for the completion of any command ran in the foreground before prompting for the next command. 


Commands ran in the background do not wait for completion. The shell prints the process ID of a background process 
when it begins. Once the background process terminates, a message showing the process ID and exit status are 
printed just before the prompt for a new command is displayed. If the user doesn't redirect the standard input or 
output for a background command, then it will be redirected to /dev/null. 

SIGNALS
    - SIGINT (CTRL-C)
        - When a foreground process receives this signal from the keyboard, the number of the signal that 
          killed it is printed out before prompting the user for the next command. 
        - The shell and any background processes ignore this signal.

    - SIGTSTP (CTRL-Z)
        - When the shell receives this signal from the keyboard, the shell toggles between two modes foreground-only 
          mode and regular-mode. After entering a mode, the shell prints out a message indicated what mode it has just 
          entered. 
            - Foreground-only mode: all subsequent commands can no longer be run in the background. All commands 
              are therefore run in the foreground. 

            - Regular-mode: commands can be run in the background. 

        - All foreground and background procesess ignore this signal.



Compiling smallsh
    gcc -o smallsh.o smallsh.c commands.c -std=c11 -Wall -Werror -g3 -O0 -lm

Running smallsh
    ./smallsh
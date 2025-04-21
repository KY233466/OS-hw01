#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_COMMANDS 30
#define MAX_ARGS 10

/* split
 * Splits a string `input` into tokens based on `delim`.
 * Stores up to `max_tokens` tokens into `tokens` array.
 *
 * Returns the number of tokens found.
 */
int split(char *input, char *tokens[], const char *delim, int max_tokens) {
    int count = 0;
    char *token = strtok(input, delim);
    while (token != NULL && count < max_tokens - 1) {
        tokens[count++] = token;
        token = strtok(NULL, delim);
    }
    tokens[count] = NULL;
    return count;
}

/* close_pipes
 * Closes all pipe file descriptors stored in `pipes`.
 * `n` is the number of file descriptors (should be 2 * (#pipes)).
 */
void close_pipes(int pipes[], int n) {
    for (int i = 0; i < n; i++) {
        close(pipes[i]);
    }
}

/* handle_error
 * Prints an error message and exits with failure.
 */
void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/* create_pipes
 * Creates the necessary pipes to support a pipeline of `command_count` 
 * commands. Stores pipe fds in the `pipes` array.
 */
void create_pipes(int pipes[], int command_count) {
    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipes + i * 2) < 0) {
            handle_error("pipe failed");
        }
    }
}

/* execute_command
 * Forks and executes a single command at index `i` in the pipeline.
 * - `command` is the command string to execute (e.g. "ls -l").
 * - `pipes` is the array of all pipe fds.
 * - `command_count` is the total number of commands in the pipeline.
 * - `pid` is where the child's PID will be stored.
 *
 * Sets up `stdin` and `stdout` using `dup2` based on pipe position.
 */
void execute_command(int i, char *command, int pipes[], int command_count, 
    pid_t *pid) {
    char *args[MAX_ARGS];
    int arg_count = split(command, args, " ", MAX_ARGS);

    // Automatically add "-h" to "ls" command
    if (strcmp(args[0], "ls") == 0) {
        args[arg_count++] = "-h";
        args[arg_count] = NULL;
    }

    *pid = fork();
    if (*pid < 0) {
        handle_error("fork failed");
    } else if (*pid == 0) {
        // Set up input from previous command
        if (i != 0) {
            if (dup2(pipes[(i - 1) * 2], STDIN_FILENO) < 0) {
                handle_error("dup2 failed (stdin)");
            }
        }

        // Set up output to next command
        if (i != command_count - 1) {
            if (dup2(pipes[i * 2 + 1], STDOUT_FILENO) < 0) {
                handle_error("dup2 failed (stdout)");
            }
        }

        close_pipes(pipes, 2 * (command_count - 1));

        // Execute the command
        execvp(args[0], args);

        // If exec fails
        fprintf(stderr, "jsh error: Command not found: %s\n", args[0]);
        exit(127);
    }
}

/* wait_for_children
 * Waits for all child processes to finish.
 * Returns the exit status of the **last command** in the pipeline.
 */
int wait_for_children(pid_t pids[], int command_count) {
    int status, last_status = 0;
    for (int i = 0; i < command_count; i++) {
        waitpid(pids[i], &status, 0);
        if (i == command_count - 1) {
            last_status = WEXITSTATUS(status);
        }
    }
    return last_status;
}

/* main
 * Main loop for the shell.
 * Reads commands from stdin, splits them into pipeline pieces, executes them,
 * and prints the status of the last command.
 */
int main() {
    char input[250];

    while (1) {
        printf("jsh$ ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("fget error");
            break;
        }

        input[strcspn(input, "\n")] = 0;  // Remove newline character
        if (strlen(input) == 0) continue;
        if (strcmp(input, "exit") == 0) break;

        // Split by pipe to get commands in the pipeline
        char *commands[MAX_COMMANDS];
        int command_count = split(input, commands, "|", MAX_COMMANDS);

        // Create pipes for N-1 transitions
        int pipes[2 * (command_count - 1)];
        create_pipes(pipes, command_count);

        // Fork and exec each command
        pid_t pids[MAX_COMMANDS];
        for (int i = 0; i < command_count; i++) {
            execute_command(i, commands[i], pipes, command_count, &pids[i]);
        }

        close_pipes(pipes, 2 * (command_count - 1));

        // Wait for all children, get last status
        int last_status = wait_for_children(pids, command_count);

        printf("jsh status: %d\n", last_status);
    }

    return 0;
}
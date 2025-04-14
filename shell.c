#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_COMMANDS 30
#define MAX_ARGS 10

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

void close_pipes(int pipes[], int n) {
    for (int i = 0; i < n; i++) {
        close(pipes[i]);
    }
}

void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void create_pipes(int pipes[], int command_count) {
    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipes + i * 2) < 0) {
            handle_error("pipe failed");
        }
    }
}

void execute_command(int i, char *command, int pipes[], int command_count, pid_t *pid) {
    char *args[MAX_ARGS];
    int arg_count = split(command, args, " ", MAX_ARGS);

    if (strcmp(args[0], "ls") == 0) {
        args[arg_count++] = "-h";
        args[arg_count] = NULL;
    }

    *pid = fork();
    if (*pid < 0) {
        handle_error("fork failed");
    } else if (*pid == 0) {
        if (i != 0) { // Not the first command: read from previous pipe
            if (dup2(pipes[(i - 1) * 2], STDIN_FILENO) < 0) {
                handle_error("dup2 failed (stdin)");
            }
        }
        if (i != command_count - 1) { // Not the last command: write to next pipe
            if (dup2(pipes[i * 2 + 1], STDOUT_FILENO) < 0) {
                handle_error("dup2 failed (stdout)");
            }
        }

        close_pipes(pipes, 2 * (command_count - 1));
        execvp(args[0], args);
        fprintf(stderr, "jsh error: Command not found: %s\n", args[0]);
        exit(127);
    }
}

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

int main() {
    char input[250];

    while (1) {
        printf("jsh$ ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("fget error");
            break;
        }

        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0) continue;
        if (strcmp(input, "exit") == 0) break;

        char *commands[MAX_COMMANDS];
        int command_count = split(input, commands, "|", MAX_COMMANDS);

        int pipes[2 * (command_count - 1)];
        create_pipes(pipes, command_count);

        pid_t pids[MAX_COMMANDS];
        for (int i = 0; i < command_count; i++) {
            execute_command(i, commands[i], pipes, command_count, &pids[i]);
        }

        close_pipes(pipes, 2 * (command_count - 1));
        int last_status = wait_for_children(pids, command_count);

        printf("jsh status: %d\n", last_status);
    }

    return 0;
}
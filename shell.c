#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main()
{
    char input[250];

    while (1)
    {
        printf("jsh$ ");
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0)
        {
            continue;
        }

        if (strcmp(input, "exit") == 0)
        {
            break;
        }

        char *commands[10];
        char *commands_separated = strtok(input, "|");
        int c_count = 0;

        while (commands_separated != NULL && c_count < 9)
        {
            commands[c_count++] = commands_separated;
            commands_separated = strtok(NULL, "|");
        }

        commands[c_count] = NULL;

        pid_t pids[10];

        for (int i = 0; i < c_count; i++)
        {
            char *args[10];
            char *arg_separated = strtok(commands[i], " ");
            int a_count = 0;

            while (arg_separated != NULL && a_count < 10 - 1)
            {
                args[a_count++] = arg_separated;
                arg_separated = strtok(NULL, " ");
            }

            if (strcmp(args[0], "ls") == 0)
            {
                args[a_count++] = "-h";
            }

            args[a_count] = NULL;

            pids[i] = fork();

            if (pids[i] < 0)
            {
                perror("Fork failed");
                exit(1);
            }
            else if (pids[i] == 0)
            {
                execvp(args[0], args);
                fprintf(stderr, "jsh error: Command not found: %s\n", args[0]);
                exit(127);
            }
        }

        int status, last_status = 0;
        for (int i = 0; i < c_count; i++)
        {
            waitpid(pids[i], &status, 0);
            if (i == c_count - 1)
            {
                last_status = WEXITSTATUS(status);
            }
        }

        printf("jsh status: %d\n", last_status);
    }

    return 0;
}
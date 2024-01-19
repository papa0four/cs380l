#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>


#define MAX_LINE 80 //MAX LINE

int main(void) {
    char **args = malloc((MAX_LINE/2 + 1) * sizeof(char*)); // COMMAND LINE ARGUMENTS
    int should_run = 1; // WHEN THE PROGRAM SHOULD EXIT
    char **path = malloc(2 * sizeof(char*)); // INITIAL SHELL PATH AS SPECIFIED
    path[0] = "/bin";
    path[1] = NULL;

    while (should_run) {
        printf("wish> ");
        fflush(stdout);

        // READ USER INPUT
        char *line = NULL;
        size_t len = 0;
        ssize_t nread = getline(&line, &len, stdin);  //USE GETLINE AS SPECIFIED

        // CHECK FOR EOF
        if (nread == -1) {
            exit(0);
        }

        // REMOVE ANY TRAILING CHARACTER
        line[strcspn(line, "\n")] = 0;

        // TOKENIZE
        char *token;
        token = strsep(&line, " ");
        int i = 0;
        while (token != NULL) {
            args[i] = token;
            i++;
            token = strsep(&line, " ");
        }
        args[i] = NULL;

        // OUTPUT REDIRECTION CHECK
        char *outputFile = NULL;
        for (i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], ">") == 0) {
                args[i] = NULL;
                outputFile = args[i+1];
                break;
            }
        }

        // BUILT IN COMMANDS
        if (strcmp(args[0], "exit") == 0) { //EXIT COMMAND
            if (args[1] != NULL) {
                fprintf(stderr, "Error: exit does not take any arguments.\n");
                continue;
            }
            should_run = 0; //IF EXIT IS ENTD, CLOSE THE SHELL
        } else if (strcmp(args[0], "cd") == 0) { //CD COMMAND
            if (args[1] == NULL || args[2] != NULL) {
                fprintf(stderr, "Error: cd takes exactly one argument.\n");
                continue;
            }
            if (chdir(args[1]) != 0) {
                perror("Error");
                continue;
            }
        } else if (strcmp(args[0], "path") == 0) {
            // FREE EXISTING PATH   5 PATH
            free(path);
            path = malloc((i+1) * sizeof(char*));
            // UPDATE THE PATH WITH THE NEW DIRECTORIES
            for (i = 0; args[i+1] != NULL; i++) {
                path[i] = args[i+1];
            }
            path[i] = NULL;
        } else {
            // CHECK IF THE COMMAND EXISTS & IT IS EXECUTABLE
            char cmd[MAX_LINE];
            int found = 0;
            for (i = 0; path[i] != NULL; i++) {
                snprintf(cmd, sizeof(cmd), "%s/%s", path[i], args[0]);
                if (access(cmd, X_OK) == 0) {
                    found = 1;
                    break;
                }
            }

            if (!found) {  //IF COMMAND IS NOT FIND PRINT ERROR
                fprintf(stderr, "Command not found.\n");
                continue;
            }

            // IF THE COMMAND IS FOUND, THEN FORK A CHILD PROCESS
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork failed.\n");
                return 1;
            } else if (pid == 0) {
                // REDIRECT OUTPUT WHERE NECESSARY
                if (outputFile != NULL) {
                    int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    if (fd == -1) {
                        perror("Error opening file for writing");
                        return 1;
                    }
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }

                // EXECUTE THE COMMAND OF THE CHILD PROCESS
                if (execv(cmd, args) < 0) {
                    fprintf(stderr, "Execution failed.\n");
                    return 1;
                }
            } else {
                // PARENT PROCESS, WAIT FOR CHILD PROCESS TO COMPLETE
                wait(NULL);
            }
        }

        free(line);
    }

    free(args);
    free(path);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
    #define PATH_SEP ";"
#else
    #define PATH_SEP ":"
#endif

int main(int argc, char *argv[]) {
    // Flush after every printf
    setbuf(stdout, NULL);

    while (1) {
        // print shell prompt
        printf("$ ");

        // get user input
        char input[256];
        fgets(input, sizeof(input), stdin);

        // handle commands
        input[strlen(input) - 1] = '\0'; // remove newline character

        char *token = strtok(input, " ");

        if (strcmp(token, "exit") == 0){
            break;
        }
        else if (strcmp(token, "echo") == 0) {
            token = strtok(NULL, " ");
            while (token != NULL) {
                printf("%s ", token);
                token = strtok(NULL, " ");
            }
            printf("\n");
        }
        else if (strcmp(token, "type") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                printf("type: missing argument\n");
                continue;
            }
            if (strcmp(token, "echo") == 0) {
                printf("%s is a shell builtin\n", token);
            } else if (strcmp(token, "exit") == 0) {
                printf("%s is a shell builtin\n", token);
            } else if (strcmp(token, "type") == 0) {
                printf("%s is a shell builtin\n", token);
            } else {
                // not a builtin command
                // check if it exists in PATH
                char *path_env = getenv("PATH");

                if (path_env == NULL) {
                    printf("%s: not found\n", token);
                    continue;
                }

                // tokenize PATH, split by PATH_SEP
                char *dir = strtok(path_env, PATH_SEP);
                int found = 0;

                // iterate through each directory in PATH to find the command
                while (dir != NULL) {
                    // build the full path to the command eg: /usr/bin/ls
                    char fullpath[512];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, token);

                    // check if the file exists and is executable
                    if (access(fullpath, X_OK) == 0) {
                        printf("%s is %s\n", token, fullpath);
                        found = 1;
                        break;
                    }

                    dir = strtok(NULL, PATH_SEP);
                }

                if (!found) {
                    printf("%s: not found\n", token);
                }
            }
        }
        else {
            printf("%s: command not found\n", input);
        }
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#ifdef _WIN32
    #define PATH_SEP ";"
#else
    #define PATH_SEP ":"
#endif

#define MAX_INPUT 256

const char *builtin_commands[] = {"exit", "echo", "type", "pwd"};
const int builtin_count = sizeof(builtin_commands) / sizeof(builtin_commands[0]);

bool is_builtin(const char *command) {
    for (int i = 0; i < builtin_count; i++) {
        if (strcmp(builtin_commands[i], command) == 0) {
            return true; 
        }
    }
    return false;
}

// search for a command in the PATH environment variable
// returns the full path if found, NULL otherwise
char *find_command_in_path(const char *command) {
    char *path_env = getenv("PATH");
    
    if (path_env == NULL) {
        return NULL;
    }

    // Copia PATH per evitare di modificare la variabile d'ambiente
    char path_copy[2048];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // tokenize PATH, split by PATH_SEP
    char *dir = strtok(path_copy, PATH_SEP);

    // iterate through each directory in PATH to find the command
    while (dir != NULL) {
        // build the full path to the command eg: /usr/bin/ls
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, command);

        // check if the file exists and is executable
        if (access(fullpath, X_OK) == 0) {
            // Ritorna una copia allocata dinamicamente del percorso
            char *result = malloc(strlen(fullpath) + 1);
            strcpy(result, fullpath);
            return result;
        }

        dir = strtok(NULL, PATH_SEP);
    }

    return NULL;
}

// return the current working directory
char *get_current_working_directory() {
    char *cwd = malloc(1024);
    if (getcwd(cwd, 1024) != NULL) {
        return cwd;
    } else {
        free(cwd);
        return NULL;
    }
}

// Handle builtin echo command
void handle_echo(void) {
    char *args = strtok(NULL, " ");
    while (args != NULL) {
        printf("%s ", args);
        args = strtok(NULL, " ");
    }
    printf("\n");
}

// Handle builtin type command
void handle_type(void) {
    // token is the command to check
    char *token = strtok(NULL, " ");
    if (is_builtin(token)) {
        printf("%s is a shell builtin\n", token);
    } else {
        char *fullpath = find_command_in_path(token);
        if (fullpath != NULL) {
            printf("%s is %s\n", token, fullpath);
            free(fullpath);
        } else {
            printf("%s: not found\n", token);
        }
    }
}

// Handle builtin pwd command
void handle_pwd(void) {
    char *cwd = get_current_working_directory();
    if (cwd != NULL) {
        printf("%s\n", cwd);
        free(cwd);
    } else {
        printf("pwd: error retrieving current directory\n");
    }
}

// Handle custom command execution
void handle_command(const char *input) {
    char input_copy[MAX_INPUT];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    char *command = strtok(input_copy, " ");
    char *fullpath = find_command_in_path(command);
    
    if (fullpath != NULL) {
        system(input);
        free(fullpath);
    } else {
        printf("%s: command not found\n", input);
    }
}

int main(int argc, char *argv[]) {
    // Flush after every printf
    setbuf(stdout, NULL);

    while (1) {
        printf("$ ");

        char input[MAX_INPUT];
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strlen(input) - 1] = '\0';

        char input_copy[MAX_INPUT];
        strncpy(input_copy, input, sizeof(input_copy) - 1);
        input_copy[sizeof(input_copy) - 1] = '\0';

        char *command = strtok(input_copy, " ");

        if (command == NULL) continue;

        if (strcmp(command, "exit") == 0) {
            break;
        } else if (strcmp(command, "echo") == 0) {
            handle_echo();
        } else if (strcmp(command, "type") == 0) {
            handle_type();
        } else if (strcmp(command, "pwd") == 0) {
            handle_pwd();
        } else {
            handle_command(input);
        }
    }

    return 0;
}

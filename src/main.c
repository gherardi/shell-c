#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>

#ifdef _WIN32
    #define PATH_SEPARATOR ";"
#else
    #define PATH_SEPARATOR ":"
#endif

#define MAX_INPUT 1024
#define MAX_ARGS 64

typedef void (*cmd_handler_t)(void);

typedef struct {
    const char *name;
    cmd_handler_t handler;
} Builtin;

void handle_exit(void);
void handle_echo(void);
void handle_type(void);
void handle_pwd(void);
void handle_cd(void);

const Builtin builtins[] = {
    {"exit", handle_exit},
    {"echo", handle_echo},
    {"type", handle_type},
    {"pwd", handle_pwd},
    {"cd", handle_cd}
};

const int builtin_count = sizeof(builtins) / sizeof(builtins[0]);

cmd_handler_t find_builtin_handler(const char *command) {
    for (int i = 0; i < builtin_count; i++) {
        if (strcmp(builtins[i].name, command) == 0) {
            return builtins[i].handler;
        }
    }
    return NULL;
}

bool is_builtin(const char *command) {
    return find_builtin_handler(command) != NULL;
}

// search for a command in the PATH environment variable
// returns the full path if found, NULL otherwise
char *find_command_in_path(const char *command) {
    char *path_env = getenv("PATH");
    
    if (path_env == NULL) return NULL;

    char path_copy[2048];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // tokenize PATH, split by PATH_SEPARATOR
    char *dir = strtok(path_copy, PATH_SEPARATOR);

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

        dir = strtok(NULL, PATH_SEPARATOR);
    }

    return NULL;
}

char *get_current_working_directory() {
    char *cwd = malloc(1024);
    if (getcwd(cwd, 1024) != NULL) {
        return cwd;
    } else {
        free(cwd);
        return NULL;
    }
}

void change_directory(const char *path) {
    if (chdir(path) != 0) {
        perror("cd");
    }
}

void handle_exit(void) {
    exit(0);
}

void handle_echo(void) {
    char *args = strtok(NULL, " ");
    while (args != NULL) {
        printf("%s ", args);
        args = strtok(NULL, " ");
    }
    printf("\n");
}

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

void handle_pwd(void) {
    char *cwd = get_current_working_directory();
    if (cwd != NULL) {
        printf("%s\n", cwd);
        free(cwd);
    } else {
        printf("pwd: error retrieving current directory\n");
    }
}

void handle_cd(void) {
    char *dir = strtok(NULL, " ");
    char expanded_path[PATH_MAX];

    if (dir == NULL) {
        printf("cd: missing argument\n");
        return;
    } else if (dir[0] == '~') {
        char *home = getenv("HOME");
        if (home != NULL) {
            snprintf(expanded_path, sizeof(expanded_path), "%s%s", home, dir + 1);
            dir = expanded_path;
        }
    }

    // check if the directory exists
    if (access(dir, F_OK) != 0) {
        printf("cd: %s: No such file or directory\n", dir);
        return;
    }

    // check if we have execute permission on the directory to enter it
    if (access(dir, X_OK) != 0) {
        printf("cd: %s: Permission denied\n", dir);
        return;
    }

    // try to change directory
    change_directory(dir);
}

// Handle custom command execution
void handle_external_command(const char *input) {
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
    // disable output buffering for stdout
    setbuf(stdout, NULL);

    while (1) {
        printf("$ ");

        char user_input[MAX_INPUT];
        if (fgets(user_input, sizeof(user_input), stdin) == NULL) break;

        user_input[strlen(user_input) - 1] = '\0'; // remove newline character that fgets adds

        if (strlen(user_input) == 0) continue;

        // make a copy of user_input to tokenize it
        char input_copy[MAX_INPUT];
        strncpy(input_copy, user_input, sizeof(input_copy) - 1);
        input_copy[sizeof(input_copy) - 1] = '\0';

        // extract the first token as command
        // strtok saves state internally for next calls
        char *command_name = strtok(input_copy, " ");

        if (command_name == NULL) continue;

        cmd_handler_t handler = find_builtin_handler(command_name);

        if (handler != NULL) {
            handler();
        } else {
            // try as an external command
            handle_external_command(user_input);
        }
    }

    return 0;
}

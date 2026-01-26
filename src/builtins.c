#include "builtins.h"
#include "executor.h" // Needed for find_command_in_path used in 'type'
#include <readline/readline.h>
#include <readline/history.h> // Required for history functions

typedef struct {
    const char *name;
    cmd_handler_t handler;
} Builtin;

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

void handle_exit(char **argv) {
    exit(0);
}

void handle_echo(char **argv) {
    for (int i = 1; argv[i] != NULL; i++) {
        printf("%s", argv[i]);
        if (argv[i + 1] != NULL) {
            printf(" ");
        }
    }
    printf("\n");
}

// In builtins.c

void handle_history(char **argv) {
    // handle "history -r <path>"
    if (argv[1] != NULL && strcmp(argv[1], "-r") == 0) {
        if (argv[2] == NULL) {
            printf("history: option requires an argument\n");
            return;
        }

        FILE *file = fopen(argv[2], "r");
        if (file == NULL) {
            printf("history: %s: cannot open history file\n", argv[2]);
            return;
        }

        char line[1024]; // buffer for reading lines
        // read the file line by line
        while (fgets(line, sizeof(line), file)) {
            // remove the trailing newline character
            line[strcspn(line, "\n")] = 0;
            
            // if the line is not empty, add it to the in-memory history
            if (strlen(line) > 0) {
                add_history(line);
            }
        }
        fclose(file);
        return; // return immediately, do not print history
    }

    // handle "history -w <path>" (write to file)
    if (argv[1] != NULL && strcmp(argv[1], "-w") == 0) {
        if (argv[2] == NULL) {
            printf("history: option requires an argument\n");
            return;
        }

        // Open with "w" mode to create file or truncate existing content
        FILE *file = fopen(argv[2], "w");
        if (file == NULL) {
            printf("history: %s: cannot open history file\n", argv[2]);
            return;
        }

        // Iterate through all history entries in memory
        for (int i = 0; i < history_length; i++) {
            HIST_ENTRY *entry = history_get(history_base + i);
            if (entry && entry->line) {
                // Write line to file with a trailing newline
                fprintf(file, "%s\n", entry->line);
            }
        }
        
        fclose(file);
        return;
    }

    // logic for printing history (with optional limit <n>)
    
    // 'history_length' and 'history_base' are global variables provided by readline
    int limit = 0;
    int start_index = 0;

    // check if a limit argument is provided (e.g., "history 5") and it's not a flag like -r
    if (argv[1] != NULL) {
        limit = atoi(argv[1]);
        
        // if the limit is valid and less than the total history length,
        // adjust the starting index to show only the last 'limit' entries.
        if (limit > 0 && limit < history_length) {
            start_index = history_length - limit;
        }
    }

    // iterate through the history entries starting from the calculated index
    for (int i = start_index; i < history_length; i++) {
        HIST_ENTRY *entry = history_get(history_base + i);
        if (entry) {
            // print the entry number and the command line
            printf("%5d  %s\n", history_base + i, entry->line);
        }
    }
}

void handle_type(char **argv) {
    if (argv[1] == NULL) {
        printf("type: missing argument\n");
        return;
    }
    
    char *token = argv[1];
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

void handle_pwd(char **argv) {
    char *cwd = get_current_working_directory();
    if (cwd != NULL) {
        printf("%s\n", cwd);
        free(cwd);
    } else {
        printf("pwd: error retrieving current directory\n");
    }
}

void handle_cd(char **argv) {
    char expanded_path[PATH_MAX];
    char *dir = argv[1];

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

    if (access(dir, F_OK) != 0) {
        printf("cd: %s: No such file or directory\n", dir);
        return;
    }

    if (access(dir, X_OK) != 0) {
        printf("cd: %s: Permission denied\n", dir);
        return;
    }

    change_directory(dir);
}

const Builtin builtins[] = {
    {"exit", handle_exit},
    {"echo", handle_echo},
    {"type", handle_type},
    {"pwd", handle_pwd},
    {"cd", handle_cd},
    {"history", handle_history}
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
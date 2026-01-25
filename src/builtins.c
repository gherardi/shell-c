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

void handle_history(char **argv) {
    // 'history_length' and 'history_base' are global variables provided by readline
    // history_length: current number of entries in history
    // history_base: the logical offset of the first entry
    
    int limit = 0;
    int start_index = 0;

    // Check if a limit argument is provided (e.g., "history 5")
    if (argv[1] != NULL) {
        limit = atoi(argv[1]);
        
        // If the limit is valid and less than the total history length,
        // adjust the starting index to show only the last 'limit' entries.
        if (limit > 0 && limit < history_length) {
            start_index = history_length - limit;
        }
        // If limit is invalid or larger than history_length, start_index remains 0 (show all)
    }

    // Iterate through the history entries starting from the calculated index
    // We use history_get() instead of history_list() for better portability (fixes macOS/libedit issues)
    for (int i = start_index; i < history_length; i++) {
        // Retrieve the entry using the absolute index
        HIST_ENTRY *entry = history_get(history_base + i);
        
        if (entry) {
            // Print the entry number and the command line
            // %5d ensures the numbers are right-aligned
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
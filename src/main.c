#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>

#ifdef _WIN32
    #define PATH_SEPARATOR ";"
#else
    #define PATH_SEPARATOR ":"
#endif

// #define MAX_INPUT 1024
#define MAX_ARGS 64

typedef void (*cmd_handler_t)(char **);

typedef struct {
    char *filename;
    int fd_type;  // 1 for stdout (>), 2 for stderr (2>), etc.
} Redirection;

typedef struct {
    char *args[MAX_ARGS];
    int count;
    Redirection output_redirect;
} Args;

typedef struct {
    const char *name;
    cmd_handler_t handler;
} Builtin;

// parse command line respecting single and double quotes
// also extracts output redirection (> or 1>)
// returns Args struct with parsed arguments
Args parse_arguments(const char *input) {
    Args args = {{NULL}, 0, {NULL, 0}};
    char buffer[MAX_INPUT];
    int buf_index = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;
    int escape_next = 0;

    for (int i = 0; input[i] != '\0' && args.count < MAX_ARGS; i++) {
        char c = input[i];

        // handle escape character (outside quotes or inside double quotes)
        if (c == '\\' && !in_single_quote) {
            // inside double quotes, only certain characters can be escaped: " $ ` \ and newline
            // outside quotes, any character following \ is treated literally
            if (i + 1 < strlen(input)) {
                char next_char = input[i + 1];
                if (in_double_quote) {
                    // inside double quotes: only escape special chars
                    if (next_char == '"' || next_char == '$' || next_char == '`' || next_char == '\\') {
                        buffer[buf_index++] = next_char;
                        i++;
                    } else {
                        // not a special char, keep the backslash
                        buffer[buf_index++] = c;
                    }
                } else {
                    // outside quotes: escape any character
                    buffer[buf_index++] = next_char;
                    i++;
                }
            }
        } else if (c == '\'' && !in_double_quote) {
            // toggle single quote (but not if inside double quotes)
            in_single_quote = !in_single_quote;
        } else if (c == '"' && !in_single_quote) {
            // toggle double quote (but not if inside single quotes)
            in_double_quote = !in_double_quote;
        } else if (isspace(c) && !in_single_quote && !in_double_quote) {
            // whitespace outside quotes - end of argument
            if (buf_index > 0) {
                buffer[buf_index] = '\0';
                args.args[args.count] = malloc(buf_index + 1);
                strcpy(args.args[args.count], buffer);
                args.count++;
                buf_index = 0;
            }
        } else {
            // regular character (or inside quotes)
            buffer[buf_index++] = c;
        }
    }

    // add last argument if any
    if (buf_index > 0) {
        buffer[buf_index] = '\0';
        args.args[args.count] = malloc(buf_index + 1);
        strcpy(args.args[args.count], buffer);
        args.count++;
    }
    
    // process redirections (>, 1>)
    for (int i = 0; i < args.count; i++) {
        if (strcmp(args.args[i], ">") == 0 || strcmp(args.args[i], "1>") == 0) {
            // next argument should be the filename
            if (i + 1 < args.count) {
                args.output_redirect.filename = args.args[i + 1];
                args.output_redirect.fd_type = 1;
                
                // remove the redirection operator and filename from args
                // shift everything down
                free(args.args[i]);  // free the > or 1>
                for (int j = i; j < args.count - 2; j++) {
                    args.args[j] = args.args[j + 2];
                }
                args.count -= 2;
                break;  // only handle first redirection for now
            }
        }
    }
    
    return args;
}

// free parsed arguments
void free_arguments(Args *args) {
    for (int i = 0; i < args->count; i++) {
        free(args->args[i]);
    }
}

void handle_exit(char **argv);
void handle_echo(char **argv);
void handle_type(char **argv);
void handle_pwd(char **argv);
void handle_cd(char **argv);

// function that wraps execution with redirection support
void execute_with_redirection(cmd_handler_t handler, char **args, const Redirection *redirect) {
    int original_fd = -1;
    
    if (redirect->filename != NULL) {
        original_fd = apply_redirection(redirect);
    }
    
    handler(args);
    
    if (original_fd >= 0) {
        restore_fd(original_fd);
    }
}

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
            // return a copy of the full path
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

// apply output redirection to a file descriptor
// returns the original fd so it can be restored later
int apply_redirection(const Redirection *redirect) {
    if (redirect->filename == NULL || redirect->fd_type == 0) {
        return -1;  // no redirection
    }
    
    int original_fd = dup(redirect->fd_type);  // save original fd
    
    int output_fd = open(redirect->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("open");
        return original_fd;
    }
    
    dup2(output_fd, redirect->fd_type);
    close(output_fd);
    
    return original_fd;
}

// restore an original file descriptor
void restore_fd(int original_fd) {
    if (original_fd >= 0) {
        dup2(original_fd, 1);  // restore stdout
        close(original_fd);
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
    // argv[0] is the command name, argv[1] onwards are the arguments
    for (int i = 1; argv[i] != NULL; i++) {
        printf("%s", argv[i]);
        if (argv[i + 1] != NULL) {
            printf(" ");
        }
    }
    printf("\n");
}

void handle_type(char **argv) {
    // argv[1] is the token to check
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

// handle custom command execution
void handle_external_command(char **argv, const Redirection *redirect) {
    if (argv[0] == NULL) return;
    
    char *fullpath = find_command_in_path(argv[0]);
    
    if (fullpath != NULL) {
        // use fork and execvp to avoid shell re-parsing arguments
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork");
        } else if (pid == 0) {
            // child process - apply redirection before executing
            if (redirect->filename != NULL) {
                apply_redirection(redirect);
            }
            
            execvp(fullpath, argv);
            // if execvp returns, there was an error
            perror(argv[0]);
            exit(1);
        } else {
            // parent process - wait for child to complete
            waitpid(pid, NULL, 0);
        }
        
        free(fullpath);
    } else {
        printf("%s: command not found\n", argv[0]);
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

        // parse arguments respecting single and double quotes
        // first argument is the command name
        Args args = parse_arguments(user_input);
        
        if (args.count == 0) continue;

        // check if it's a builtin command
        cmd_handler_t handler = find_builtin_handler(args.args[0]);

        if (handler != NULL) {
            // call handler with argv array and redirection info
            execute_with_redirection(handler, (char **)args.args, &args.output_redirect);
        } else {
            // try as an external command
            handle_external_command((char **)args.args, &args.output_redirect);
        }
        
        // clean up allocated arguments
        free_arguments(&args);
    }

    return 0;
}

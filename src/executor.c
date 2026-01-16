#include "executor.h"

int apply_redirection(const Redirection *redirect) {
    if (redirect->filename == NULL || redirect->fd_type == 0) {
        return -1;
    }
    
    int original_fd = dup(redirect->fd_type);
    
    int flags = O_WRONLY | O_CREAT;
    if (redirect->append) {
        flags |= O_APPEND;
    } else {
        flags |= O_TRUNC;
    }
    
    int output_fd = open(redirect->filename, flags, 0644);
    if (output_fd < 0) {
        perror("open");
        return original_fd;
    }
    
    dup2(output_fd, redirect->fd_type);
    close(output_fd);
    
    return original_fd;
}

void restore_fd(int original_fd, int fd_type) {
    if (original_fd >= 0) {
        dup2(original_fd, fd_type);
        close(original_fd);
    }
}

void execute_with_redirection(cmd_handler_t handler, char **args, const Redirection *redirect) {
    int original_fd = -1;
    
    if (redirect->filename != NULL) {
        original_fd = apply_redirection(redirect);
    }
    
    handler(args);
    
    if (original_fd >= 0) {
        restore_fd(original_fd, redirect->fd_type);
    }
}

char *find_command_in_path(const char *command) {
    char *path_env = getenv("PATH");
    if (path_env == NULL) return NULL;

    char path_copy[2048];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *dir = strtok(path_copy, PATH_SEPARATOR);

    while (dir != NULL) {
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, command);

        if (access(fullpath, X_OK) == 0) {
            char *result = malloc(strlen(fullpath) + 1);
            strcpy(result, fullpath);
            return result;
        }

        dir = strtok(NULL, PATH_SEPARATOR);
    }

    return NULL;
}

void handle_external_command(char **argv, const Redirection *redirect) {
    if (argv[0] == NULL) return;
    
    char *fullpath = find_command_in_path(argv[0]);
    
    if (fullpath != NULL) {
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork");
        } else if (pid == 0) {
            // child process
            if (redirect->filename != NULL) {
                apply_redirection(redirect);
            }
            
            execvp(fullpath, argv);
            perror(argv[0]);
            exit(1);
        } else {
            // parent process
            waitpid(pid, NULL, 0);
        }
        
        free(fullpath);
    } else {
        printf("%s: command not found\n", argv[0]);
    }
}
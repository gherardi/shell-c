#include "pipeline.h"
#include "parser.h"
#include "executor.h"
#include "builtins.h"
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

// Check if the input contains a pipeline operator (|)
// Returns 1 if pipeline is found, 0 otherwise
int has_pipeline(const char *input) {
    if (input == NULL) return 0;
    
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        
        // Handle escape character
        if (c == '\\' && !in_single_quote) {
            if (i + 1 < strlen(input)) {
                i++; // Skip next character
            }
            continue;
        }
        
        // Handle quotes
        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (c == '|' && !in_single_quote && !in_double_quote) {
            return 1; // Found pipeline operator
        }
    }
    
    return 0;
}

// Split input at the first pipeline operator
// Returns the position of the | character, or -1 if not found
static int find_pipeline_position(const char *input) {
    if (input == NULL) return -1;
    
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        
        // Handle escape character
        if (c == '\\' && !in_single_quote) {
            if (i + 1 < strlen(input)) {
                i++; // Skip next character
            }
            continue;
        }
        
        // Handle quotes
        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (c == '|' && !in_single_quote && !in_double_quote) {
            return i; // Found pipeline operator
        }
    }
    
    return -1;
}

// Execute a pipeline with two commands
void execute_pipeline(const char *input) {
    if (input == NULL) return;
    
    // Find the pipeline operator position
    int pipe_pos = find_pipeline_position(input);
    if (pipe_pos < 0) {
        return; // No pipeline found
    }
    
    // Split input into left and right commands
    int left_len = pipe_pos;
    int right_start = pipe_pos + 1;
    
    // Skip whitespace after |
    while (input[right_start] != '\0' && isspace(input[right_start])) {
        right_start++;
    }
    
    // Allocate memory for left and right command strings
    char *left_cmd = malloc(left_len + 1);
    char *right_cmd = malloc(strlen(input) - right_start + 1);
    
    if (left_cmd == NULL || right_cmd == NULL) {
        perror("malloc");
        if (left_cmd) free(left_cmd);
        if (right_cmd) free(right_cmd);
        return;
    }
    
    // Copy left command (trim trailing whitespace)
    int left_end = left_len - 1;
    while (left_end >= 0 && isspace(input[left_end])) {
        left_end--;
    }
    strncpy(left_cmd, input, left_end + 1);
    left_cmd[left_end + 1] = '\0';
    
    // Copy right command
    strcpy(right_cmd, input + right_start);
    
    // Parse both commands
    Args left_args = parse_arguments(left_cmd);
    Args right_args = parse_arguments(right_cmd);
    
    // Free temporary command strings
    free(left_cmd);
    free(right_cmd);
    
    // Check if both commands are valid
    if (left_args.count == 0 || right_args.count == 0) {
        free_arguments(&left_args);
        free_arguments(&right_args);
        return;
    }
    
    // Create pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        free_arguments(&left_args);
        free_arguments(&right_args);
        return;
    }
    
    // Fork first process (left command)
    pid_t left_pid = fork();
    if (left_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free_arguments(&left_args);
        free_arguments(&right_args);
        return;
    }
    
    if (left_pid == 0) {
        // Child process for left command
        // Close read end of pipe
        close(pipefd[0]);
        
        // Redirect stdout to write end of pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        // Check if it's a builtin
        cmd_handler_t handler = find_builtin_handler(left_args.args[0]);
        
        if (handler != NULL) {
            // Execute builtin directly (pipe redirection already set up with dup2)
            handler((char **)left_args.args);
            free_arguments(&left_args);
            free_arguments(&right_args);
            exit(0);
        } else {
            // Execute external command
            char *fullpath = find_command_in_path(left_args.args[0]);
            if (fullpath != NULL) {
                execvp(fullpath, (char **)left_args.args);
                perror(left_args.args[0]);
                free(fullpath);
            } else {
                printf("%s: command not found\n", left_args.args[0]);
            }
            free_arguments(&left_args);
            free_arguments(&right_args);
            exit(1);
        }
    }
    
    // Fork second process (right command)
    pid_t right_pid = fork();
    if (right_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free_arguments(&left_args);
        free_arguments(&right_args);
        return;
    }
    
    if (right_pid == 0) {
        // Child process for right command
        // Close write end of pipe
        close(pipefd[1]);
        
        // Redirect stdin from read end of pipe
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        // Apply output redirection if specified
        int original_fd = -1;
        if (right_args.output_redirect.filename != NULL) {
            original_fd = apply_redirection(&right_args.output_redirect);
        }
        
        // Check if it's a builtin
        cmd_handler_t handler = find_builtin_handler(right_args.args[0]);
        
        if (handler != NULL) {
            // Execute builtin directly (pipe and file redirection already set up)
            handler((char **)right_args.args);
            if (original_fd >= 0) {
                restore_fd(original_fd, right_args.output_redirect.fd_type);
            }
            free_arguments(&left_args);
            free_arguments(&right_args);
            exit(0);
        } else {
            // Execute external command
            char *fullpath = find_command_in_path(right_args.args[0]);
            if (fullpath != NULL) {
                execvp(fullpath, (char **)right_args.args);
                perror(right_args.args[0]);
                free(fullpath);
            } else {
                printf("%s: command not found\n", right_args.args[0]);
            }
            if (original_fd >= 0) {
                restore_fd(original_fd, right_args.output_redirect.fd_type);
            }
            free_arguments(&left_args);
            free_arguments(&right_args);
            exit(1);
        }
    }
    
    // Parent process
    // Close both ends of pipe in parent
    close(pipefd[0]);
    close(pipefd[1]);
    
    // Wait for both child processes to complete
    waitpid(left_pid, NULL, 0);
    waitpid(right_pid, NULL, 0);
    
    // Free arguments
    free_arguments(&left_args);
    free_arguments(&right_args);
}

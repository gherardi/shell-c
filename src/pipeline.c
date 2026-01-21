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

// Find all pipeline operator positions
// Returns the number of pipeline operators found, and stores positions in positions array
static int find_all_pipeline_positions(const char *input, int *positions, int max_positions) {
    if (input == NULL) return 0;
    
    int count = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    for (int i = 0; input[i] != '\0' && count < max_positions; i++) {
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
            positions[count++] = i;
        }
    }
    
    return count;
}

// Execute a pipeline with multiple commands
void execute_pipeline(const char *input) {
    if (input == NULL) return;
    
    // Find all pipeline positions
    int pipe_positions[64]; // Maximum 64 pipeline operators
    int pipe_count = find_all_pipeline_positions(input, pipe_positions, 64);
    
    if (pipe_count == 0) {
        return; // No pipeline found
    }
    
    int num_commands = pipe_count + 1;
    
    // Split input into command strings
    char **commands = malloc(num_commands * sizeof(char *));
    if (commands == NULL) {
        perror("malloc");
        return;
    }
    
    // Extract first command
    int start = 0;
    for (int i = 0; i < num_commands; i++) {
        int end;
        if (i < pipe_count) {
            end = pipe_positions[i];
        } else {
            end = strlen(input);
        }
        
        // Trim leading whitespace
        while (start < end && isspace(input[start])) {
            start++;
        }
        
        // Trim trailing whitespace
        int cmd_end = end - 1;
        while (cmd_end >= start && isspace(input[cmd_end])) {
            cmd_end--;
        }
        
        if (cmd_end < start) {
            // Empty command
            commands[i] = NULL;
        } else {
            int cmd_len = cmd_end - start + 1;
            commands[i] = malloc(cmd_len + 1);
            if (commands[i] == NULL) {
                perror("malloc");
                // Free already allocated commands
                for (int j = 0; j < i; j++) {
                    if (commands[j]) free(commands[j]);
                }
                free(commands);
                return;
            }
            strncpy(commands[i], input + start, cmd_len);
            commands[i][cmd_len] = '\0';
        }
        
        // Move start to after the pipe operator
        if (i < pipe_count) {
            start = pipe_positions[i] + 1;
        }
    }
    
    // Parse all commands
    Args *args_array = malloc(num_commands * sizeof(Args));
    if (args_array == NULL) {
        perror("malloc");
        for (int i = 0; i < num_commands; i++) {
            if (commands[i]) free(commands[i]);
        }
        free(commands);
        return;
    }
    
    for (int i = 0; i < num_commands; i++) {
        if (commands[i] == NULL) {
            args_array[i].count = 0;
            args_array[i].output_redirect.filename = NULL;
        } else {
            args_array[i] = parse_arguments(commands[i]);
        }
    }
    
    // Free command strings
    for (int i = 0; i < num_commands; i++) {
        if (commands[i]) free(commands[i]);
    }
    free(commands);
    
    // Validate all commands
    for (int i = 0; i < num_commands; i++) {
        if (args_array[i].count == 0) {
            // Invalid command, clean up and return
            for (int j = 0; j < num_commands; j++) {
                free_arguments(&args_array[j]);
            }
            free(args_array);
            return;
        }
    }
    
    // Create pipes (n commands need n-1 pipes
    int num_pipes = num_commands - 1;
    int (*pipefds)[2] = malloc(num_pipes * sizeof(int[2]));
    if (pipefds == NULL) {
        perror("malloc");
        for (int i = 0; i < num_commands; i++) {
            free_arguments(&args_array[i]);
        }
        free(args_array);
        return;
    }
    
    // Create all pipes
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds[i]) == -1) {
            perror("pipe");
            // Close already created pipes
            for (int j = 0; j < i; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            for (int j = 0; j < num_commands; j++) {
                free_arguments(&args_array[j]);
            }
            free(pipefds);
            free(args_array);
            return;
        }
    }
    
    // Fork processes for each command
    pid_t *pids = malloc(num_commands * sizeof(pid_t));
    if (pids == NULL) {
        perror("malloc");
        for (int i = 0; i < num_pipes; i++) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
        }
        for (int i = 0; i < num_commands; i++) {
            free_arguments(&args_array[i]);
        }
        free(pipefds);
        free(args_array);
        return;
    }
    
    // Fork all processes
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("fork");
            // Close all pipes
            for (int j = 0; j < num_pipes; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            // Wait for already forked processes
            for (int j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }
            for (int j = 0; j < num_commands; j++) {
                free_arguments(&args_array[j]);
            }
            free(pids);
            free(pipefds);
            free(args_array);
            return;
        }
        
        if (pids[i] == 0) {
            // Child process for command i
            
            // Close all pipe ends that this process doesn't need
            for (int j = 0; j < num_pipes; j++) {
                // Close read end of pipe j if not the one we need (i-1)
                if (j != i - 1) {
                    close(pipefds[j][0]);
                }
                // Close write end of pipe j if not the one we need (i)
                if (j != i) {
                    close(pipefds[j][1]);
                }
            }
            
            // Set up stdin redirection
            if (i > 0) {
                // Not the first command: read from previous pipe
                dup2(pipefds[i - 1][0], STDIN_FILENO);
                close(pipefds[i - 1][0]);
            }
            // First command uses original stdin (no redirection needed)
            
            // Set up stdout redirection
            int original_fd = -1;
            if (i < num_commands - 1) {
                // Not the last command: write to next pipe
                dup2(pipefds[i][1], STDOUT_FILENO);
                close(pipefds[i][1]);
            } else {
                // Last command: apply output redirection if specified
                if (args_array[i].output_redirect.filename != NULL) {
                    original_fd = apply_redirection(&args_array[i].output_redirect);
                }
            }
            
            // Execute the command (same logic for all commands)
            cmd_handler_t handler = find_builtin_handler(args_array[i].args[0]);
            
            if (handler != NULL) {
                // Execute builtin
                handler((char **)args_array[i].args);
                if (original_fd >= 0) {
                    restore_fd(original_fd, args_array[i].output_redirect.fd_type);
                }
                // Free all arguments
                for (int j = 0; j < num_commands; j++) {
                    free_arguments(&args_array[j]);
                }
                free(args_array);
                free(pipefds);
                free(pids);
                exit(0);
            } else {
                // Execute external command
                char *fullpath = find_command_in_path(args_array[i].args[0]);
                if (fullpath != NULL) {
                    execvp(fullpath, (char **)args_array[i].args);
                    perror(args_array[i].args[0]);
                    free(fullpath);
                } else {
                    printf("%s: command not found\n", args_array[i].args[0]);
                }
                if (original_fd >= 0) {
                    restore_fd(original_fd, args_array[i].output_redirect.fd_type);
                }
                // Free all arguments
                for (int j = 0; j < num_commands; j++) {
                    free_arguments(&args_array[j]);
                }
                free(args_array);
                free(pipefds);
                free(pids);
                exit(1);
            }
        }
    }
    
    // Parent process: close all pipe ends
    for (int i = 0; i < num_pipes; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }
    
    // Wait for all child processes to complete
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
    
    // Free all resources
    for (int i = 0; i < num_commands; i++) {
        free_arguments(&args_array[i]);
    }
    free(args_array);
    free(pipefds);
    free(pids);
}

#include "parser.h"

Args parse_arguments(const char *input) {
    Args args = {{NULL}, 0, {NULL, 0}};
    char buffer[MAX_INPUT];
    int buf_index = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;

    for (int i = 0; input[i] != '\0' && args.count < MAX_ARGS; i++) {
        char c = input[i];

        // handle escape character
        if (c == '\\' && !in_single_quote) {
            if (i + 1 < strlen(input)) {
                char next_char = input[i + 1];
                if (in_double_quote) {
                    if (next_char == '"' || next_char == '$' || next_char == '`' || next_char == '\\') {
                        buffer[buf_index++] = next_char;
                        i++;
                    } else {
                        buffer[buf_index++] = c;
                    }
                } else {
                    buffer[buf_index++] = next_char;
                    i++;
                }
            }
        } else if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (isspace(c) && !in_single_quote && !in_double_quote) {
            if (buf_index > 0) {
                buffer[buf_index] = '\0';
                args.args[args.count] = malloc(buf_index + 1);
                strcpy(args.args[args.count], buffer);
                args.count++;
                buf_index = 0;
            }
        } else {
            buffer[buf_index++] = c;
        }
    }

    if (buf_index > 0) {
        buffer[buf_index] = '\0';
        args.args[args.count] = malloc(buf_index + 1);
        strcpy(args.args[args.count], buffer);
        args.count++;
    }
    
    // process redirections
    for (int i = 0; i < args.count; i++) {
        int fd_type = 0;
        int append = 0;
        
        // check for output redirection symbols
        if (strcmp(args.args[i], ">") == 0 || strcmp(args.args[i], "1>") == 0) {
            fd_type = 1;
            append = 0;
        } else if (strcmp(args.args[i], ">>") == 0 || strcmp(args.args[i], "1>>") == 0) {
            fd_type = 1;
            append = 1;
        } else if (strcmp(args.args[i], "2>") == 0) {
            fd_type = 2;
            append = 0;
        } else if (strcmp(args.args[i], "2>>") == 0) {
            fd_type = 2;
            append = 1;
        }

        // if a redirection is found
        if (fd_type != 0) {
            if (i + 1 < args.count) {
                args.output_redirect.filename = malloc(strlen(args.args[i + 1]) + 1);
                strcpy(args.output_redirect.filename, args.args[i + 1]);
                args.output_redirect.fd_type = fd_type;
                args.output_redirect.append = append;
                
                free(args.args[i]);
                free(args.args[i + 1]);
                
                for (int j = i; j < args.count - 2; j++) {
                    args.args[j] = args.args[j + 2];
                }
                args.count -= 2;
                break;
            }
        }
    }
    
    args.args[args.count] = NULL;
    return args;
}

void free_arguments(Args *args) {
    // free each argument string
    for (int i = 0; i < args->count; i++) {
        free(args->args[i]);
    }
    // free redirection filename if allocated
    if (args->output_redirect.filename != NULL) {
        free(args->output_redirect.filename);
    }
}
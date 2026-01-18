#include "common.h"
#include "parser.h"
#include "builtins.h"
#include "executor.h"
#include <readline/readline.h>
#include <readline/history.h>

// generator function for builtin command completion
char *builtin_generator(const char *text, int state) {
    static int list_index, len;
    static const char *builtins[] = {"echo", "exit"};
    static const int builtin_count = 2;
    
    // initialize on first call
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    
    // return next matching builtin
    while (list_index < builtin_count) {
        const char *name = builtins[list_index++];
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    
    return NULL;
}

// completion generator function
char **command_completion(const char *text, int start, int end) {
    char **matches = NULL;
    
    // suppress unused parameter warning
    (void)end;
    
    // disable default filename completion
    rl_attempted_completion_over = 1;
    
    // only complete the first word (command name)
    if (start == 0) {
        matches = rl_completion_matches(text, builtin_generator);
    }
    
    return matches;
}

int main(int argc, char *argv[]) {
    // disable output buffering for stdout
    setbuf(stdout, NULL);
    
    // set up tab completion
    rl_attempted_completion_function = command_completion;
    // append space after completion
    rl_completion_append_character = ' ';

    while (1) {
        char *user_input = readline("$ ");
        
        if (user_input == NULL) break; // EOF (Ctrl+D)
        
        // skip empty input
        if (strlen(user_input) == 0) {
            free(user_input);
            continue;
        }

        Args args = parse_arguments(user_input);
        
        if (args.count == 0) {
            free(user_input);
            continue;
        }
        
        cmd_handler_t handler = find_builtin_handler(args.args[0]);
        
        if (handler != NULL) {
            execute_with_redirection(handler, (char **)args.args, &args.output_redirect);
        } else {
            handle_external_command((char **)args.args, &args.output_redirect);
        }
        
        free_arguments(&args);
        free(user_input);
    }

    return 0;
}

#include "common.h"
#include "parser.h"
#include "builtins.h"
#include "executor.h"
#include "completion.h"
#include "pipeline.h"
#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char *argv[]) {
    // disable output buffering for stdout
    setbuf(stdout, NULL);
    
    // set up tab completion
    setup_completion();

    // load history from HISTFILE on startup
    char *histfile = getenv("HISTFILE");
    if (histfile != NULL) {
        FILE *file = fopen(histfile, "r");
        if (file != NULL) {
            char line[1024];
            while (fgets(line, sizeof(line), file)) {
                // remove trailing newline
                line[strcspn(line, "\n")] = 0;
                if (strlen(line) > 0) {
                    add_history(line);
                }
            }
            fclose(file);
        }
    }

    while (1) {
        char *user_input = readline("$ ");
        
        if (user_input == NULL) {
            // EOF (Ctrl+D): save history before exiting
            save_history_to_file();
            break;
        }
        
        // skip empty input
        if (strlen(user_input) == 0) {
            free(user_input);
            continue;
        }

        // Add input to history
        add_history(user_input);

        // Check for pipeline first
        if (has_pipeline(user_input)) {
            execute_pipeline(user_input);
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

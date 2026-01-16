#include "common.h"
#include "parser.h"
#include "builtins.h"
#include "executor.h"

int main(int argc, char *argv[]) {
    // disable output buffering for stdout
    setbuf(stdout, NULL);

    while (1) {
        printf("$ ");

        char user_input[MAX_INPUT];
        if (fgets(user_input, sizeof(user_input), stdin) == NULL) break;

        user_input[strlen(user_input) - 1] = '\0'; // remove newline

        if (strlen(user_input) == 0) continue;

        Args args = parse_arguments(user_input);
        
        if (args.count == 0) continue;

        cmd_handler_t handler = find_builtin_handler(args.args[0]);

        if (handler != NULL) {
            execute_with_redirection(handler, (char **)args.args, &args.output_redirect);
        } else {
            handle_external_command((char **)args.args, &args.output_redirect);
        }
        
        free_arguments(&args);
    }

    return 0;
}

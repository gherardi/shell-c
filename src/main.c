#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Flush after every printf
    setbuf(stdout, NULL);

    while (1) {
        // print shell prompt
        printf("$ ");

        // get user input
        char input[256];
        fgets(input, sizeof(input), stdin);

        // handle commands
        input[strlen(input) - 1] = '\0'; // remove newline character

        char *token = strtok(input, " ");

        if (strcmp(token, "exit") == 0){
            break;
        }
        else if (strcmp(token, "echo") == 0) {
            token = strtok(NULL, " ");
            while (token != NULL) {
                printf("%s ", token);
                token = strtok(NULL, " ");
            }
            printf("\n");
        }
        else {
            printf("%s: command not found\n", input);
        }
    }

    return 0;
}

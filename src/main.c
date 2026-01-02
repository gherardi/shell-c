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

    
    // handle invalid commands
    input[strlen(input) - 1] = '\0'; // remove newline character
    if (strcmp(input, "exit") == 0) break; // exit command
    printf("%s: command not found\n", input);
  }

  return 0;
}

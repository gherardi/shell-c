#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  printf("$ ");

  // get user input
  char input[256];
  fgets(input, sizeof(input), stdin);

  // handle invalid commands
  input[strlen(input) - 1] = '\0'; // remove newline character
  printf("%s: command not found\n", input);

  return 0;
}

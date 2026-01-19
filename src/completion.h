#ifndef COMPLETION_H
#define COMPLETION_H

#include "common.h"
#include <readline/readline.h>

char **command_completion(const char *text, int start, int end);
void setup_completion(void);

#endif
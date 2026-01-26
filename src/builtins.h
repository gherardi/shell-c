#ifndef BUILTINS_H
#define BUILTINS_H

#include "common.h"

void handle_exit(char **argv);
void handle_echo(char **argv);
void handle_type(char **argv);
void handle_pwd(char **argv);
void handle_cd(char **argv);
void handle_history(char **argv);

void save_history_to_file(void);

cmd_handler_t find_builtin_handler(const char *command);
bool is_builtin(const char *command);

#endif
#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "common.h"

char *find_command_in_path(const char *command);
void execute_with_redirection(cmd_handler_t handler, char **args, const Redirection *redirect);
void handle_external_command(char **args, const Redirection *redirect);
int apply_redirection(const Redirection *redirect);
void restore_fd(int original_fd, int fd_type);

#endif
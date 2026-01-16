#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>

#ifdef _WIN32
    #define PATH_SEPARATOR ";"
#else
    #define PATH_SEPARATOR ":"
#endif

#define MAX_INPUT 1024
#define MAX_ARGS 64

// Tipo per i puntatori a funzione dei comandi builtin
typedef void (*cmd_handler_t)(char **);

typedef struct {
    char *filename;
    int fd_type;  // 1 per stdout (>), 2 per stderr (2>), ecc.
    int append;   // 1 per append (>>), 0 per truncate (>)
} Redirection;

typedef struct {
    char *args[MAX_ARGS];
    int count;
    Redirection output_redirect;
} Args;

#endif
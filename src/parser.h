#ifndef PARSER_H
#define PARSER_H

#include "common.h"

Args parse_arguments(const char *input);
void free_arguments(Args *args);

#endif
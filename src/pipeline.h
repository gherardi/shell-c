#ifndef PIPELINE_H
#define PIPELINE_H

#include "common.h"

// Check if the input contains a pipeline operator (|)
int has_pipeline(const char *input);

// Execute a pipeline with two commands
void execute_pipeline(const char *input);

#endif

#ifndef OPTIONS_H
#define OPTIONS_H

#include "ls.h"

void options_init(Options *options);
int parse_options(int argc, char **argv, Options *options, int *first_operand);
void print_usage(const char *program_name);

#endif

// Command Line Interface: Lida com os comandos digitados no terminal

#ifndef CLI_H
#define CLI_H

#include <stdio.h>

struct trace_options {
    char **target_argv;
    int raw_events;
};


int parse_args(int argc, char **argv, struct trace_options *opts);
void print_usage(FILE *out, const char *prog);

#endif

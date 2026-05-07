#ifndef STUDENT_API_H
#define STUDENT_API_H

#include <stddef.h>
#include <stdio.h>

#include "syscall_event.h"
#include "trace_runtime.h"

#define MAX_FILTER_NAMES 32
#define MAX_SYSCALL_TRACKED 512

// define a estrutura do pairer
struct syscall_pairer {
    int has_entry;
    struct syscall_event entry;
};

int student_pair_syscall(struct syscall_pairer *pairer,
                         const struct syscall_event *ev,
                         struct syscall_event *out);

void student_format_event(const struct syscall_event *ev,
                          char *buf,
                          size_t bufsz);

void student_debug_raw_event(const struct syscall_event *ev,
                             char *buf,
                             size_t bufsz);

#endif

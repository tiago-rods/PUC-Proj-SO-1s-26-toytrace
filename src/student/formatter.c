#include "student_api.h"
#include "trace_helpers.h"
#include "syscall_names.h"
#include <sys/syscall.h>
#include <stdio.h>

void student_debug_raw_event(const struct syscall_event *ev,
                             char *buf,
                             size_t bufsz)
{
    /*
     * Para uma melhor visualização dos primeiros argumentos,
     * organizamos dentro de um if para responder a pergunta
     * "por que a mesma syscall aparece duas vezes?". Pois temos
     * a entrada e a saída de uma syscall sendo registrada por
     * ptrace
     */
    if (ev->entering) {
        // Na entrada, mostramos os primeiros argumentos
        snprintf(buf, bufsz, "pid=%d %s entrada arg0=%#lx arg1=%#lx",
                 ev->pid,
                 syscall_name(ev->syscall_no),
                 ev->args[0],
                 ev->args[1]);
    } else {
        // Na saída, mostramos o que a syscall devolveu
        snprintf(buf, bufsz, "pid=%d %s saida ret=%ld",
                 ev->pid,
                 syscall_name(ev->syscall_no),
                 ev->ret);
    }
}

void student_format_event(const struct syscall_event *ev,
                          char *buf,
                          size_t bufsz)
{
    char path[256];

    // Trata os argumentos dependendo de qual é a syscall
    switch (ev->syscall_no) {            
        case SYS_read:
        case SYS_write:
            // write/read(fd, buf, count) = ret
            snprintf(buf, bufsz, "%s(%ld, %#lx, %lu) = %ld", 
                syscall_name(ev->syscall_no), 
                (long)ev->args[0],
                ev->args[1],
                (unsigned long)ev->args[2],
                ev->ret);
            break;
        
        case SYS_openat: 
        // openat(dirfd, "path", flags, mode) = ret
            if (read_child_string(ev->pid, ev->args[1], path, sizeof(path)) < 0)
                snprintf(path, sizeof(path), "<ilegivel>");
            snprintf(buf, bufsz, "%s(%ld, \"%s\", %#lx, %#lx) = %ld",
                syscall_name(ev->syscall_no),
                (long)ev->args[0],
                path,
                ev->args[2],
                ev->args[3],
                ev->ret);
            break;

        case SYS_execve: 
        // execve("path", ...) = ret
            if (read_child_string(ev->pid, ev->args[0], path, sizeof(path)) < 0)
                snprintf(path, sizeof(path), "<ilegivel>");
            snprintf(buf, bufsz, "%s(\"%s\", ...) = %ld",
                syscall_name(ev->syscall_no),
                path,
                ev->ret);
            break;
        // exit_group(status) = ret
        case SYS_exit_group:
            snprintf(buf, bufsz, "%s(%ld) = %ld",
                syscall_name(ev->syscall_no),
                (long)ev->args[0],
                ev->ret);
            break;

        default:
            snprintf(buf, bufsz, "%s(%#lx, %#lx, %#lx, %#lx, %#lx, %#lx) = %ld",
            syscall_name(ev->syscall_no),
            ev->args[0],
            ev->args[1],
            ev->args[2],
            ev->args[3],
            ev->args[4],
            ev->args[5],
            ev->ret);
            break;
    }
}

#include "student_api.h"

#include "syscall_names.h"

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
    /*
     * TODO Semana 5:
     *
     * Primeiro, formate uma syscall completa em uma linha simples.
     *
     * Depois, adicione casos especiais para:
     *     read(fd, buf, count)
     *     write(fd, buf, count)
     *     openat(dirfd, "path", flags, mode)
     *     execve("path", ...)
     *     exit_group(status)
     *
     * Para caminhos do processo monitorado, use read_child_string().
     * Se a leitura falhar, imprima "<ilegivel>".
     */
    snprintf(buf, bufsz, "%s(%#lx, %#lx, %#lx, %#lx, %#lx, %#lx) = %ld",
             syscall_name(ev->syscall_no),
             ev->args[0],
             ev->args[1],
             ev->args[2],
             ev->args[3],
             ev->args[4],
             ev->args[5],
             ev->ret);
}

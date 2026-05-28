#include "student_api.h"

#include "syscall_names.h"

#include <stdio.h>
#include <string.h> // usado para usar strcmp

void student_debug_raw_event(const struct syscall_event *ev,
                             char *buf,
                             size_t bufsz)
{
    /*
     * Suporte de depuracao para a Semana 4:
     *
     * Esta funcao existe para inspecionar eventos crus depois que o runtime
     * ja consegue parar em syscalls e preencher struct syscall_event.
     * Ela nao e a formatacao final do projeto.
     *
     * Experimento sugerido:
     * - imprima o nome da syscall;
     * - imprima se o evento e entrada ou saida;
     * - imprima o pid;
     * - em eventos de entrada, observe os argumentos;
     * - em eventos de saida, observe o valor de retorno.
     *
     * Depois compare a saida de:
     *
     *   ./toytrace trace --raw-events -- ./tests/targets/hello_write
     *
     * A pergunta importante da Semana 4 e:
     * por que a mesma syscall aparece duas vezes?
     */
    snprintf(buf, bufsz, "pid=%d %s %s",
             ev->pid,
             syscall_name(ev->syscall_no),
             ev->entering ? "entrada" : "saida");
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
    const char *sys_name = syscall_name(ev->syscall_no);
    char path_buf[1024];
    const char *path;

    if (strcmp(sys_name, "read") == 0) {
        snprintf(buf, bufsz, "read(%ld, %#lx, %ld) = %ld",
                 (long)ev->args[0], ev->args[1], (long)ev->args[2], ev->ret);
    } else if (strcmp(sys_name, "write") == 0) {
        snprintf(buf, bufsz, "write(%ld, %#lx, %ld) = %ld",
                 (long)ev->args[0], ev->args[1], (long)ev->args[2], ev->ret);
    } else if (strcmp(sys_name, "openat") == 0) {
        path = "<ilegivel>";
        // Em openat, o pathname fica no argumento 1 (args[1])
        if (read_child_string(ev->pid, ev->args[1], path_buf, sizeof(path_buf)) == 0) {
            path = path_buf;
        }
        snprintf(buf, bufsz, "openat(%ld, \"%s\", %ld, %ld) = %ld",
                 (long)ev->args[0], path, (long)ev->args[2], (long)ev->args[3], ev->ret);
    } else if (strcmp(sys_name, "execve") == 0) {
        path = "<ilegivel>";
        // Em execve, o pathname fica no argumento 0 (args[0])
        if (read_child_string(ev->pid, ev->args[0], path_buf, sizeof(path_buf)) == 0) {
            path = path_buf;
        }
        snprintf(buf, bufsz, "execve(\"%s\", ...) = %ld", path, ev->ret);
    } else if (strcmp(sys_name, "exit_group") == 0) {
        snprintf(buf, bufsz, "exit_group(%ld) = %ld",
                 (long)ev->args[0], ev->ret);
    } else {
        // Formatação genérica para syscalls não tratadas
        snprintf(buf, bufsz, "%s(%#lx, %#lx, %#lx, %#lx, %#lx, %#lx) = %ld",
                 sys_name, ev->args[0], ev->args[1], ev->args[2],
                 ev->args[3], ev->args[4], ev->args[5], ev->ret);
    }
}
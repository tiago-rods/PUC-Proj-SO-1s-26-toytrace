#include "trace_runtime.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#if !defined(__x86_64__)
#error "Este runtime didatico suporta apenas Linux x86_64."
#endif

static void fill_event_from_regs(pid_t pid,
                                 int entering,
                                 const struct user_regs_struct *regs,
                                 struct syscall_event *ev)
{
    /*
     * TODO Semana 4:
     *
     * Preencha struct syscall_event usando os registradores x86_64.
     *
     * Dicas:
     * - regs->orig_rax contem o numero da syscall.
     * - regs->rax contem o retorno, valido na saida.
     * - os seis argumentos ficam em rdi, rsi, rdx, r10, r8 e r9.
     * - ev->entering deve copiar o parametro entering.
     */
    memset(ev, 0, sizeof(*ev));
    ev->pid = pid;
    ev->entering = entering;
}

static pid_t launch_tracee(char *const argv[])
{
    /*
     * TODO Semana 2:
     *
     * Crie o processo monitorado.
     *
     * Fluxo esperado:
     * - fork()
     * - no filho:
     *   - ptrace(PTRACE_TRACEME, ...)
     *   - raise(SIGSTOP)
     *   - execvp(argv[0], argv)
     * - no pai:
     *   - retornar o pid do filho
     *
     * Em erro, imprima uma mensagem com perror() e retorne -1.
     */
    pid_t child = fork();
    if (child < 0) {
        perror("Erro ao criar processo filho: ");
        return -1;
    }

    if (child == 0) {
        /* Filho */
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("Erro ao rastrear processo filho");
            _exit(1);
        }
        raise(SIGSTOP);
        execvp(argv[0], argv);
        perror("Erro ao executar");
        _exit(1);
    }

    /* Pai */
    return child;

}

static int wait_for_initial_stop(pid_t child)
{
    /*
     * TODO Semana 2:
     *
     * O filho chama raise(SIGSTOP) antes de executar o programa alvo.
     * O pai precisa esperar essa parada inicial com waitpid().
     *
     * Retorne 0 se o filho parou como esperado, -1 em erro.
     */
    int status;
    if (waitpid(child, &status, 0) < 0) {
        perror("Erro ao esperar parada inicial");
        return -1;
    }

    if (WIFSTOPPED(status)) {
        return 0;
    }

    return -1;
}

static int configure_trace_options(pid_t child)
{
    /*
     * TODO Semana 3:
     *
     * Configure PTRACE_O_TRACESYSGOOD com PTRACE_SETOPTIONS.
     * Isso ajuda a diferenciar paradas de syscall de outros sinais.
     */
    if (ptrace(PTRACE_SETOPTIONS, child, NULL, PTRACE_O_TRACESYSGOOD) < 0) {
        perror("Erro em ptrace(SETOPTIONS)");
        return -1;
    }
    return 0; 
}

static int resume_until_next_syscall(pid_t child, int signal_to_deliver)
{
    /*
     * TODO Semana 3:
     *
     * Use ptrace(PTRACE_SYSCALL, ...) para deixar o filho executar ate a
     * proxima entrada ou saida de syscall.
     *
     * signal_to_deliver deve ser repassado como quarto argumento do ptrace.
     */
    if (ptrace(PTRACE_SYSCALL, child, NULL, signal_to_deliver) < 0) {
        perror("Erro em ptrace(SYSCALL)");
        return -1;
    }
    return 0;
}

static int wait_for_syscall_stop(pid_t child, int *status)
{
    do {
        if (waitpid(child, status, 0) < 0) {
            perror("Erro em waitpid");
            return -1;
        }

        if (WIFEXITED(*status) || WIFSIGNALED(*status)) {
            return 0; // Fim do processo
        }

        if (WSTOPSIG(*status) == (SIGTRAP | 0x80)) {
            return 1; // Parada de syscall confirmada
        }

        // se chegar aqui, capturamos uma parada que não é de syscall
        // retomamos enrtegando o sinal de volta ao filho 
    } while (resume_until_next_syscall(child, WSTOPSIG(*status)) == 0);

    return -1;
}

int trace_program(char *const argv[],
                  trace_observer_fn observer,
                  void *userdata)
{
    pid_t child;
    int status = 0;
    int entering = 1;

    if (argv == NULL || argv[0] == NULL) {
        fprintf(stderr, "erro: programa alvo ausente\n");
        return -1;
    }

    child = launch_tracee(argv);
    if (child < 0) {
        return -1;
    }

    if (wait_for_initial_stop(child) < 0) {
        return -1;
    }

    if (configure_trace_options(child) < 0) {
        return -1;
    }

    if (resume_until_next_syscall(child, 0) < 0) {
        return -1;
    }

    // Consome a parada de saída do execve, dessa forma o loop começará na entrada da primeira syscall
    // não sei se isso é 100% necessário, mas resolve o problema de chamar o observer uma vez a mais no binário.c
    if (wait_for_syscall_stop(child, &status) < 0) {
        return -1;
    }

    while (1) {
        struct user_regs_struct regs;
        struct syscall_event ev;
        int stop_kind;

        stop_kind = wait_for_syscall_stop(child, &status);
        if (stop_kind <= 0) {
            if (stop_kind == 0) {
                if (WIFEXITED(status)) {
                    return WEXITSTATUS(status);
                }
                if (WIFSIGNALED(status)) {
                    return 128 + WTERMSIG(status);
                }
            }
            return stop_kind;
        }

        /*
         * TODO Semana 4:
         *
         * Use PTRACE_GETREGS para preencher regs.
         * Depois chame fill_event_from_regs() e observer().
         */
        memset(&regs, 0, sizeof(regs));
        fill_event_from_regs(child, entering, &regs, &ev);
        if (observer != NULL) {
            observer(&ev, userdata);
        }

        entering = !entering;

        if (resume_until_next_syscall(child, 0) < 0) {
            return -1;
        }
    }
}

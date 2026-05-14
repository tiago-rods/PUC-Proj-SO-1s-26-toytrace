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
    memset(ev, 0, sizeof(*ev));
    ev->pid      = pid;
    ev->entering = entering;

    /*
     * orig_rax guarda o número da syscall tanto na entrada quanto na saída.
     * rax na entrada ainda contém o número da syscall; na saída contém o
     * valor de retorno (possivelmente negativo em caso de erro).
     */
    ev->syscall_no = (long)regs->orig_rax;
    ev->ret        = (long)regs->rax;

    /*
     * Convenção de chamada x86_64 para syscalls (diferente de funções C):
     *   arg0 → rdi
     *   arg1 → rsi
     *   arg2 → rdx
     *   arg3 → r10   (não rcx como em funções C)
     *   arg4 → r8
     *   arg5 → r9
     */
    ev->args[0] = regs->rdi;
    ev->args[1] = regs->rsi;
    ev->args[2] = regs->rdx;
    ev->args[3] = regs->r10;
    ev->args[4] = regs->r8;
    ev->args[5] = regs->r9;
}

static pid_t launch_tracee(char *const argv[])
{
    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        return -1;
    }

    if (child == 0) {
        /* Filho */

        /* Avisa o kernel que este processo aceita ser monitorado pelo pai. */
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("ptrace TRACEME");
            _exit(1);
        }

        /*
         * Suspende o filho antes de executar o programa alvo.
         * O pai vai esperar essa parada em wait_for_initial_stop(),
         * configurar as opcoes de trace e so entao liberar o filho.
         */
        raise(SIGSTOP);

        /*
         * Substitui a imagem do filho pelo programa alvo.
         * Se execvp retornar, houve erro (programa nao encontrado, etc.).
         */
        execvp(argv[0], argv);
        perror("execvp");
        _exit(1);
    }

    /* Pai */
    return child;
}

static int wait_for_initial_stop(pid_t child)
{
    int status;

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    /*
     * O filho deve ter parado por causa do raise(SIGSTOP) em launch_tracee.
     * Qualquer outra situacao e inesperada.
     */
    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) {
        fprintf(stderr, "erro: parada inicial inesperada (status=%d)\n", status);
        return -1;
    }

    return 0;
}

static int configure_trace_options(pid_t child)
{
    if (ptrace(PTRACE_SETOPTIONS, child, NULL,
               (void *)(long)PTRACE_O_TRACESYSGOOD) < 0) {
        perror("ptrace(PTRACE_SETOPTIONS)");
        return -1;
    }

    return 0;
}

static int resume_until_next_syscall(pid_t child, int signal_to_deliver)
{
    if (ptrace(PTRACE_SYSCALL, child, NULL,
               (void *)(long)signal_to_deliver) < 0) {
        perror("ptrace(PTRACE_SYSCALL)");
        return -1;
    }

    return 0;
}

static int wait_for_syscall_stop(pid_t child, int *status)
{
    int sig;

    if (waitpid(child, status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    /* Processo encerrou normalmente ou por sinal */
    if (WIFEXITED(*status) || WIFSIGNALED(*status)) {
        return 0;
    }

    if (!WIFSTOPPED(*status)) {
        /* Estado inesperado — não deveria acontecer */
        return -1;
    }

    sig = WSTOPSIG(*status);

    /* Com PTRACE_O_TRACESYSGOOD, syscall-stops têm o bit 0x80 setado */
    if (sig == (SIGTRAP | 0x80)) {
        return 1;  /* é uma parada de syscall */
    }

    /*
     * Parada por outro sinal (ex: SIGTRAP de execve, SIGCHLD etc.).
     * SIGTRAP "puro" vem do execve — não deve ser reentregue ao filho.
     * Para outros sinais reais, o chamador pode repassá-los via
     * signal_to_deliver em resume_until_next_syscall().
     * Por ora, avança sem entregar o sinal de volta.
     */
    return 1;
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

    while (1) {
        struct user_regs_struct regs;
        struct syscall_event ev;
        int stop_kind;

        stop_kind = wait_for_syscall_stop(child, &status);
        if (stop_kind < 0) {
            return -1;
        }
        if (stop_kind == 0) {
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
            return 0;
        }

        /* Semana 4: lê os registradores do filho neste ponto de parada */
        if (ptrace(PTRACE_GETREGS, child, NULL, &regs) < 0) {
            perror("ptrace(PTRACE_GETREGS)");
            return -1;
        }

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
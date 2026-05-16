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
    ev->syscall_no = regs->orig_rax;    // Número da syscall
    ev->ret = regs->rax;                // Valor de retorno, relevante na saída
    ev->args[0] = regs->rdi;
    ev->args[1] = regs->rsi;
    ev->args[2] = regs->rdx;            // Argumentos
    ev->args[3] = regs->r10;
    ev->args[4] = regs->r8;
    ev->args[5] = regs->r9;
    ev->pid = pid;
    ev->entering = entering;            // Indica a entrada ou saída
}

static pid_t launch_tracee(char *const argv[])
{
    pid_t child = fork();

    if (child < 0) {
        perror("Erro no fork");
        return -1;
    }

    if (child == 0) {
        // Filho pede para ser rastrado pelo pai
        if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("Erro no ptrace");
            _exit(1);
        }

        // Para o filho
        raise(SIGSTOP);

        // O filho então executa o que foi atribuído a ele
        execvp(argv[0], argv);
        // Se isso retornar, quer dizer que ocorreu algum erro
        perror("Erro ao executar");
        _exit(1);
    }

    // Pai retorna o filho
    return child;
}

static int wait_for_initial_stop(pid_t child)
{
    // Pai espera o filho
    int status;
    if (waitpid(child, &status, 0) < 0) {
        perror("Erro no waitpid");
        return -1;
    }

    // Filho tem que estar parado por SIGSTOP em launch_tracee
    // Se não, então há algo de errado, então retornamos -1
    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) {
        fprintf(stderr, "Erro inesperado. status=%d\n", status);
        return -1;
    }

    return 0;
}

static int configure_trace_options(pid_t child)
{
    /* Configuramos o rastreamento do filho com PTRACE_O_TRACESYSGOOD
     * Ou seja, evita que todo SIGTRAP seja tratado como syscall
     */
    if ((ptrace(PTRACE_SETOPTIONS, child, NULL, PTRACE_O_TRACESYSGOOD)) < 0) {
        perror("Erro no ptrace");
        return -1;
    }
    return 0;
}

static int resume_until_next_syscall(pid_t child, int signal_to_deliver)
{
    // Permissão para o filho continuar até a próxima entrada ou saída de syscall
    if ((ptrace(PTRACE_SYSCALL, child, NULL, signal_to_deliver)) < 0) {
        perror("Erro no ptrace");
        return -1;
    }
    return 0;
}

static int wait_for_syscall_stop(pid_t child, int *status)
{
    int sinal_para_repassar = 0;
    do {
        // Pai espera a próxima mudança de estado
        if (waitpid(child, status, 0) < 0) {
            perror("Erro no waitpid");
            return -1;
        }

        // Filho morreu ou terminou normalmente
        if (WIFEXITED(*status) || WIFSIGNALED(*status)) {
            return 0;
        }
        
        // O filho parou e o pai precisa decidir por quê. 
        if (WIFSTOPPED(*status)) {

            int sig = WSTOPSIG(*status);

            // Verifica o sinal da syscall
            if (sig & 0x80) {
                return 1;
            }

            /* "Uma parada comum por SIGTRAP não deve ser reenviada ao filho. Outros sinais
             *podem ser repassados no próximo PTRACE_SYSCALL."
             */
             // Ou seja, temos que decidir o que passar:
            sinal_para_repassar = (sig == SIGTRAP) ? 0 : sig;
        }
    } while(resume_until_next_syscall(child, sinal_para_repassar) == 0);

    perror("Erro na verificacao do status");
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

    // Aqui que criamos o filho e pedimos para rastreá-lo
    child = launch_tracee(argv);
    if (child < 0) {
        return -1;
    }

    // Processo pai verifica se o filho está parado em SIGSTOP
    if (wait_for_initial_stop(child) < 0) {
        return -1;
    }

    // Configuração das opções de rastreamento do filho
    if (configure_trace_options(child) < 0) {
        return -1;
    }

    // Permite o filho executar até a próxima entrada ou saída de syscall
    if (resume_until_next_syscall(child, 0) < 0) {
        return -1;
    }

    while (1) {
        struct user_regs_struct regs;
        struct syscall_event ev;
        int stop_kind;

        // Pausa a execução do pai e espera o filho fazer uma syscall
        stop_kind = wait_for_syscall_stop(child, &status);
        if (stop_kind < 0) {
            return -1;
        }

        // Se stop_kind for 0, o processo filho terminou a sua vida útil (não é syscall)
        if (stop_kind == 0) {
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
            return 0;
        }

        // Limpa o cache de regs
        memset(&regs, 0, sizeof(regs));

        // Recebe os registradores da syscall
        ptrace(PTRACE_GETREGS, child, NULL, &regs);

        // Preenche a struct event com os registradores
        fill_event_from_regs(child, entering, &regs, &ev);
        if (observer != NULL) {
            observer(&ev, userdata);
        }

        // Inverte a fase da syscall para a próxima parada (entrada ou saída)
        entering = !entering;

        // Libera o filho do congelamento para ele continuar rodando até a próxima syscall
        if (resume_until_next_syscall(child, 0) < 0) {
            return -1;
        }
    }
}

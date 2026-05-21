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
    ev->pid = pid;
    ev->entering = entering;
    ev->syscall_no = (long)regs->orig_rax;
    ev->ret = (long)regs->rax;
    ev->args[0] = regs->rdi;
    ev->args[1] = regs->rsi;
    ev->args[2] = regs->rdx;
    ev->args[3] = regs->r10;
    ev->args[4] = regs->r8;
    ev->args[5] = regs->r9;
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
        perror("nao foi possivel fazer o fork()");
        return -1;
    }

    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("nao foi possivel fazer o ptrace");
            _exit(1);
        }
        raise(SIGSTOP);
        execvp(argv[0], argv);
        perror("nao foi possivel fazer execvp"); // Esta linha só será atingida se o execvp falhar
        _exit(1);
    }

    return child; // Processo pai retorna o PID
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
        perror("waitpid");
        return -1;
    }
    
    // Depois do waitpid, o pai deve verificar se o filho realmente parou: WIFSTOPPED(status)
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
    // Com PTRACE_O_TRACESYSGOOD, paradas causadas por syscall aparecem com obit 0x80 ligado no sinal. 
    if (ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD) < 0) {
        perror("configure_trace_options (ptrace PTRACE_SETOPTIONS)");
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
    /*
    O quarto argumento permite entregar um sinal pendente ao filho. Na maior parte
    do projeto, ele será 0. Se o filho parar por um sinal real que não seja uma parada
    comum de SIGTRAP, esse sinal pode ser repassado nessa posição
    */
    if (ptrace(PTRACE_SYSCALL, child, 0, signal_to_deliver) < 0) {
        perror("resume_until_next_syscall (ptrace PTRACE_SYSCALL)");
        return -1;
    }
    return 0;
}

static int wait_for_syscall_stop(pid_t child, int *status)
{
    /*
     * TODO Semana 3:
     *
     * Espere o filho com waitpid().
     *
     * Retorne:
     *   1 se a parada foi uma parada de syscall;
     *   0 se o filho terminou normalmente ou por sinal;
     *  -1 em erro.
     *
     * Dicas:
     * - WIFEXITED e WIFSIGNALED indicam fim do processo.
     * - WIFSTOPPED indica que o processo parou.
     * - com PTRACE_O_TRACESYSGOOD, syscall-stops aparecem com bit 0x80.
     * - paradas SIGTRAP comuns nao devem ser entregues de volta ao filho.
     */

     // Faz o waitpid e verifica o motivo da parada. Depois que o filho continua com
     // PTRACE_SYSCALL, o pai espera a próxima mudança de estado:
    while (1) {
        if (waitpid(child, status, 0) < 0) {
            perror("wait_for_syscall_stop (waitpid)");
            return -1;
        }

        // Se o filho finaliza a execução (via WIFEXITED ou WIFSIGNALED), retornamos 0
        if (WIFEXITED(*status) || WIFSIGNALED(*status)) {
            return 0;
        }

        if (WIFSTOPPED(*status)) {
            int sig = WSTOPSIG(*status);
            
            if (sig == (SIGTRAP | 0x80)) {
                return 1;
            }
            
            int signal_to_deliver = (sig == SIGTRAP) ? 0 : sig;
            if (resume_until_next_syscall(child, signal_to_deliver) < 0) {
                return -1;
            }
        }
    }
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

        /*
         * TODO Semana 4:
         *
         * Use PTRACE_GETREGS para preencher regs.
         * Depois chame fill_event_from_regs() e observer().
         */
        memset(&regs, 0, sizeof(regs));
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

#include "cli.h"
#include "student_api.h"
#include "trace_runtime.h"

#include <stdio.h>

/* 
 * Estrutura que mantém o estado global do rastreador durante a execução.
 * É passada para o callback 'trace_observer' via ponteiro userdata.
 */
struct trace_state {
    int raw_events;               // Flag para imprimir eventos crus (debug)
    struct syscall_pairer pairer; // Mantém o estado de pareamento das syscalls (entrada/saída)
};

/* 
 * Função de Callback (Observador): chamada pelo motor de runtime a cada evento de syscall.
 * ev: Dados do evento capturado (registradores, número da syscall, etc).
 * userdata: Ponteiro para o nosso 'trace_state' definido no main.
 */
static void trace_observer(const struct syscall_event *ev, void *userdata)
{
    struct trace_state *state = userdata;
    struct syscall_event completed;
    char line[512];
    int ready;

    // Se o modo de eventos crus estiver ativo, apenas formata e imprime o evento atual
    if (state->raw_events) {
        student_debug_raw_event(ev, line, sizeof(line));
        puts(line);
        return;
    }

    // Tenta parear a entrada com a saída da syscall. 
    // Retorna 1 se o par estiver completo e pronto para ser exibido.
    ready = student_pair_syscall(&state->pairer, ev, &completed);
    if (ready <= 0) {
        return; // Ainda aguardando o par (ex: capturou apenas a entrada)
    }

    // Com o par completo, chama a função do estudante para formatar a linha final
    student_format_event(&completed, line, sizeof(line));
    puts(line); // Imprime a linha formatada no terminal
}

int main(int argc, char **argv)
{
    struct trace_options opts;    // Opções vindas da linha de comando
    struct trace_state state = {0}; // Estado interno do programa
    int rc;

    // 1. Processa os argumentos da linha de comando
    rc = parse_args(argc, argv, &opts);
    if (rc > 0) {
        return 0; // Usuário pediu help ou argumentos válidos mas programa deve encerrar (ex: --help)
    }
    if (rc < 0) {
        return 2; // Erro de sintaxe nos argumentos
    }

    // 2. Configura o estado inicial baseado nas opções da CLI
    state.raw_events = opts.raw_events;

    // 3. Inicia o rastreamento do programa alvo.
    // Passa a lista de argumentos do alvo, a função de callback e o ponteiro de estado.
    rc = trace_program(opts.target_argv, trace_observer, &state);

    // Retorna o código de saída apropriado
    return rc < 0 ? 1 : rc;
}

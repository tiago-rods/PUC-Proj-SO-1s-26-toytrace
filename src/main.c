#include "cli.h"
#include "student_api.h"
#include "trace_runtime.h"

#include <stdio.h>

struct trace_state {
    // Diz se o usuário pediu para imprimir os eventos no modo cru.
    int raw_events;
    // Essa estrutura está em student_api.h e funciona como uma memória de verificação de eventos para guardar os entados de entrada e saída controlando o processo
    struct syscall_pairer pairer;
};

/*
Toda vez que o programa-alvo tentar fazer uma chamada de sistema 
(como um read, write, open), o Sistema Operacional pausa ele e avisa o motor.
O motor então chama a sua função trace_observer(), passando os detalhes do 
evento (ev). É uma callback.
*/

static void trace_observer(const struct syscall_event *ev, void *userdata)
{
    struct trace_state *state = userdata;
    struct syscall_event completed;
    char line[512];
    int ready;

    //  Se o modo "cru" (raw) estiver ativado, a função vai receber o evento ev (que é apenas a entrada ou apenas a saída de uma syscall) e formatá-lo como um texto simples dentro do buffer line. Provavelmente extrairá o ID da syscall e os registradores básicos nesse modo
    if (state->raw_events) {
        student_debug_raw_event(ev, line, sizeof(line));
        puts(line);
        return;
    }

    // Quando um programa faz uma syscall, o sistema (via ptrace) costuma emitir dois eventos: um de entrada (quando a syscall é solicitada) e um de saída (quando a syscall termina e retorna um valor). E eles são etregues pelo motor separados um de cada vez
    // Quando receber o evento de saída correspondente, ele junta as duas informações numa coisa só (a variável completed), e retorna 1 (dizendo: "pronto, casei as informações!").
    ready = student_pair_syscall(&state->pairer, ev, &completed);
    if (ready <= 0) {
        return;
    }

    // formatar a string final bonita, semelhante ao strace real. Exemplo: read(0, "texto", 5) = 5. Precisará ler os argumentos da syscall e o valor de retorno.
    student_format_event(&completed, line, sizeof(line));
    puts(line);
}

// lê os argumentos 
int main(int argc, char **argv)
{
    struct trace_options opts;
    struct trace_state state = {0};
    int rc;


    // Usando a CLI.h para a função
    rc = parse_args(argc, argv, &opts);
    if (rc > 0) {
        return 0;
    }
    if (rc < 0) {
        return 2;
    }

    // pega da biblioteca trace_runtime.h dando partida no rastreamento
    state.raw_events = opts.raw_events;
    // entrega para a função: o programa a ser executado, a função aqui criada tarce_observer e o state
    rc = trace_program(opts.target_argv, trace_observer, &state);
    return rc < 0 ? 1 : rc;
}

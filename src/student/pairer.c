#include "student_api.h"

int student_pair_syscall(struct syscall_pairer *pairer,
                         const struct syscall_event *ev,
                         struct syscall_event *out)
{
    /*
     * TODO Semana 2:
     *
     * O runtime chama esta funcao duas vezes para cada syscall:
     *
     *   1. uma vez antes da syscall executar
     *   2. uma vez depois da syscall terminar
     *
     * Na primeira parada, os argumentos estao disponiveis.
     * Na segunda parada, o retorno esta disponivel.
     *
     * Seu trabalho e produzir um evento completo apenas quando ja existirem
     * as duas metades da syscall.
     *
     * Dicas:
     * - ev->entering == 1 indica entrada de syscall.
     * - ev->entering == 0 indica saida de syscall.
     * - para comecar, assuma apenas um processo monitorado.
     *
     * Retorne:
     *   1 se out contem uma syscall completa
     *   0 se ainda nao ha syscall completa
     *  -1 se a sequencia de eventos parece invalida
     */

    if (ev->entering) {
        /* Primeira metade: salva o evento de entrada e aguarda a saída */
        pairer->entry = *ev;
        pairer->has_entry = 1;
        return 0;
    }

    /* Segunda metade: evento de saída */
    if (!pairer->has_entry) {
        /* Saída sem entrada prévia — sequência inválida */
        return -1;
    }

    /* Monta o evento completo: argumentos da entrada + retorno da saída */
    *out = pairer->entry;
    out->entering = 0;
    out->ret = ev->ret;

    /* Reseta o estado para a próxima syscall */
    pairer->has_entry = 0;

    return 1;
}

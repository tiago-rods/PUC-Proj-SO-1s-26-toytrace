#include "student_api.h"

int student_pair_syscall(struct syscall_pairer *pairer,
                         const struct syscall_event *ev,
                         struct syscall_event *out)
{
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
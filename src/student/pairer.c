#include "student_api.h"
#include <string.h>

int student_pair_syscall(struct syscall_pairer *pairer,
                         const struct syscall_event *ev,
                         struct syscall_event *out) {
  /*
   * TODO Semana 5:
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
    /* Fase de entrada: chegou uma nova syscall */

    if (pairer->has_entry) {
      /* Ja tinhamos uma entrada pendente sem saida -> sequencia invalida */
      return -1;
    }

    /* Guarda os dados de entrada para usar depois */
    pairer->entry = *ev;
    pairer->has_entry = 1;

    return 0; /* evento ainda incompleto */

  } else {
    /* Fase de saida: syscall terminou, temos o valor de retorno */

    if (!pairer->has_entry) {
      /* Saida sem entrada correspondente -> sequencia invalida */
      return -1;
    }

    /* Monta o evento completo combinando entrada + saida */
    out->pid = pairer->entry.pid;
    out->syscall_no = pairer->entry.syscall_no;
    out->entering = 0;
    out->ret = ev->ret;
    memcpy(out->args, pairer->entry.args, sizeof(out->args));

    /* Limpa o estado para a proxima syscall */
    pairer->has_entry = 0;

    return 1; /* evento completo disponivel em *out */
  }
}

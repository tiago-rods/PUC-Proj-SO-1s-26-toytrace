#include "student_api.h"

int student_pair_syscall(struct syscall_pairer *pairer,
                         const struct syscall_event *ev,
                         struct syscall_event *out)
{
    // É uma syscall
     if (ev->entering) {
        // Entrada pendente sem ter ocorrido a saída
        if(pairer->has_entry){
            return -1;
        }
        // Agora temos uma syscall em "espera" (Esperando sua saída)
        pairer->has_entry = 1;
        pairer->entry = *ev;     // E guardamos os dados da syscall

        return 0;      // Como não uma syscall completa, então retornamos 0
    } else {
        if (pairer->has_entry == 0) {
            return -1;      // Sequência inválida, sycall saindo sem ter uma entrada
        }
        // Copia a entrada para a variável 'out'
        *out = pairer->entry;
        out->ret = ev->ret;     // Valor de retorno
        out->entering = 0;      // Atualiza o status para finalização da syscall
        pairer->has_entry = 0;  // próxima syscall liberada 
        // 1 se out contem uma syscall completa
        return 1;
    }
}

#ifndef TRACE_RUNTIME_H
#define TRACE_RUNTIME_H

// Pega a struct de event
#include "syscall_event.h"

// É uma função de callback (observadora) que é chamada sempre que um evento de syscall acontece durante o tracing do programa
// Com  o typedef só precisa chamar trace_observer_fn que ele cria uma função com os parâmetros (evento e dados)
typedef void (*trace_observer_fn)(const struct syscall_event *ev, void *userdata);

//Executa um programa alvo (passado via argv) e intercepta todas as chamadas de sistema (syscalls) que ele faz, chamando a função observer para cada evento detectado.
int trace_program(char *const argv[],
                  trace_observer_fn observer,
                  void *userdata);
// trace_observer_fn observer é o callback que ela chama sempre que detecta um evento de syscall

#endif

#ifndef SYSCALL_EVENT_H
#define SYSCALL_EVENT_H


#include <sys/types.h> // Biblioteca que ganha acesso a vários tipos de dados terminados em _t (que significa type)

// define a estrutura de eventos muito usada durante o programa todo
struct syscall_event {
    pid_t pid;
    int entering;              /* 1 na entrada da syscall, 0 na saida */
    long syscall_no;           // numero da syscall
    long ret;                  /* valido apenas em eventos de saida */
    unsigned long args[6];     /* argumentos capturados na entrada */
};

#endif

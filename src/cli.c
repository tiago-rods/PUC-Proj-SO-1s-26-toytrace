#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Esse arquivo tem a função exclusiva de entender e organizar o que você digita no terminal quando vai executar o seu rastreador


/*
O padrão que esse programa adotou para separar os comandos é o uso de dois traços --. Por exemplo, se você digita: 
toytrace trace --raw-events -- echo "ola" Tudo 
que está antes do -- pertence ao seu rastreador. Tudo que vem depois 
(echo "ola") é o programa-alvo que será executado e monitorado. Essa função 
simplesmente varre a lista de argumentos digitados no terminal e retorna a posição (o índice) exata de onde esse -- está. Se não encontrar, retorna -1.
*/
static int find_separator(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            return i;
        }
    }

    return -1;
}

void print_usage(FILE *out, const char *prog)
{
    fprintf(out,
            "uso: %s trace [opcoes] -- programa [argumentos...]\n"
            "\n"
            "opcoes:\n"
            "  --raw-events    Semana 4: imprime eventos crus de entrada/saida\n"
            "\n"
            "exemplos:\n"
            "  %s trace -- /bin/echo oi\n"
            "  %s trace --raw-events -- ./tests/targets/hello_write\n",
            prog, prog, prog);
}

int parse_args(int argc, char **argv, struct trace_options *opts)
{
    int sep;
    int i;

    // deixa memoria para opts (onde as configs serao guardadas)
    memset(opts, 0, sizeof(*opts));

    // verifica se o user noa digitou corretamente e mostra o manual de help
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(stdout, argv[0]);
        return 1;
    }

    // ve se a primeira palavra no comando foi trace 
    if (strcmp(argv[1], "trace") != 0) {
        fprintf(stderr, "erro: comando desconhecido: %s\n", argv[1]);
        return -1;
    }

    // ve se usuário colocou o -- e informou um programa-alvo logo em seguida.
    sep = find_separator(argc, argv);
    if (sep < 0 || sep + 1 >= argc) {
        fprintf(stderr, "erro: informe o programa alvo depois de --\n");
        return -1;
    }

    // Olha todos os argumentos que vieram antes do --. Atualmente, ela só reconhece a opção --raw-events. Se achar ela, marca a flag opts->raw_events = 1. Se achar qualquer outra coisa ali no meio, acusa que a opção é desconhecida.
    for (i = 2; i < sep; i++) {
        if (strcmp(argv[i], "--raw-events") == 0) {
            opts->raw_events = 1;
        } else {
            fprintf(stderr, "erro: opcao desconhecida: %s\n", argv[i]);
            return -1;
        }
    }

    // Pega os args depois de -- e os guarda em opts. Esse ponteiro é repassado para o trace_runtime conseguir iniciar o programa que o usuário quer rastrear
    opts->target_argv = &argv[sep + 1];
    return 0;
}

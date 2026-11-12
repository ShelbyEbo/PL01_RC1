#include "../headers/header.h"

int main(int argc, char **argv)
{
    Node node;
    int opt;

    memset(&node, 0, sizeof(Node));
    strncpy(node.boot_ip, DEFAULT_BOOT_IP, sizeof(node.boot_ip) - 1);
    node.boot_ip[sizeof(node.boot_ip) - 1] = '\0';
    node.boot_port = DEFAULT_BOOT_PORT;
    node.ring_port = 0;

    while ((opt = getopt(argc, argv, "t:i:p:")) != -1)
    {
        switch (opt)
        {
            case 't':
                node.ring_port = atoi(optarg);
                break;
            case 'i':
                strncpy(node.boot_ip, optarg, sizeof(node.boot_ip) - 1);
                node.boot_ip[sizeof(node.boot_ip) - 1] = '\0';
                break;
            case 'p':
                node.boot_port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Uso: %s [-t ringport] [-i bootIP] [-p bootport]\n", argv[0]);
                return (EXIT_FAILURE);
        }
    }

    /* Detetar o endereço IP da máquina local */
    if (get_local_ip(node.boot_ip, node.boot_port, node.ip, sizeof(node.ip)) != 0)
    {
        strncpy(node.ip, "127.0.0.1", sizeof(node.ip) - 1);
        node.ip[sizeof(node.ip) - 1] = '\0';
    }

    /* Inicializar o servidor TCP na porta ring_port */
    if (init_tcp_server(&node) < 0)
    {
        fprintf(stderr, "Falha crítica: Não foi possível iniciar o servidor TCP.\n");
        return (EXIT_FAILURE);
    }

    printf("Servidor TCP activo em %s:%d\n", node.ip, node.ring_port);

    /* Iniciar o ciclo de eventos multiplexado com select() */
    run_event_loop(&node);

    /* Limpeza final de sockets ao encerrar */
    close_tcp_server(&node);
    return (0);
}

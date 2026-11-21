#include "../headers/header.h"

static void init_node_defaults(Node *node)
{
    memset(node, 0, sizeof(Node));
    strncpy(node->boot_ip, DEFAULT_BOOT_IP, sizeof(node->boot_ip) - 1);
    node->boot_ip[sizeof(node->boot_ip) - 1] = '\0';
    node->boot_port = DEFAULT_BOOT_PORT;
    node->ring_port = 0;
}

static int parse_args(int argc, char **argv, Node *node)
{
    int opt;

    while ((opt = getopt(argc, argv, "t:i:p:")) != -1)
    {
        if (opt == 't')
            node->ring_port = atoi(optarg);
        else if (opt == 'i')
        {
            strncpy(node->boot_ip, optarg, sizeof(node->boot_ip) - 1);
            node->boot_ip[sizeof(node->boot_ip) - 1] = '\0';
        }
        else if (opt == 'p')
            node->boot_port = atoi(optarg);
        else
        {
            fprintf(stderr, "Uso: %s [-t ringport] [-i bootIP] [-p bootport]\n",
                argv[0]);
            return (-1);
        }
    }
    return (0);
}

int main(int argc, char **argv)
{
    Node node;

    /* Escrever para um vizinho que fechou a sessão não pode matar o processo:
     * o erro passa a ser devolvido por send() (EPIPE) e tratado. */
    signal(SIGPIPE, SIG_IGN);
    init_node_defaults(&node);
    if (parse_args(argc, argv, &node) < 0)
        return (EXIT_FAILURE);
    if (get_local_ip(node.boot_ip, node.boot_port, node.ip,
            sizeof(node.ip)) != 0)
    {
        strncpy(node.ip, "127.0.0.1", sizeof(node.ip) - 1);
        node.ip[sizeof(node.ip) - 1] = '\0';
    }
    if (init_tcp_server(&node) < 0)
    {
        fprintf(stderr,
            "Falha crítica: Não foi possível iniciar o servidor TCP.\n");
        return (EXIT_FAILURE);
    }
    printf("Servidor TCP activo em %s:%d\n", node.ip, node.ring_port);
    run_event_loop(&node);
    close_tcp_server(&node);
    return (0);
}

#include "../headers/header.h"

void node_reset(Node *node)
{
    node->joined = 0;
    node->is_boot = 0;
    node->ring_id = 0;
    node->node_id = 0;
    node->pred_id = 0;
    node->succ_id = 0;
    node->pred_ip[0] = '\0';
    node->succ_ip[0] = '\0';
    node->pred_tcp = 0;
    node->succ_tcp = 0;
    pending_clear(node);
}

void leave_alone(Node *node)
{
    if (bootstrap_unregister(node, node->ring_id) == 0)
        printf("Anel %d anulado no Bootstrap Server com sucesso.\n", node->ring_id);
    else
        printf("Aviso: Falha ao anular registo no Bootstrap Server.\n");
    node_reset(node);
    printf("Nó abandonou o anel.\n");
}

void leave_multiple(Node *node)
{
    if (node->is_boot)
    {
        if (bootstrap_register(node, node->ring_id, node->succ_id, node->succ_ip, node->succ_tcp) != 0)
            printf("Aviso: falha ao actualizar o nó de arranque no Bootstrap Server.\n");
        if (send_line(node->succ.fd, "BOOT") < 0)
            printf("Aviso: não foi possível enviar BOOT ao sucessor.\n");
    }
    conn_close(&node->succ);

    if (send_line(node->pred.fd, "CON %d %s %d", node->succ_id, node->succ_ip, node->succ_tcp) < 0)
        printf("Aviso: não foi possível enviar CON ao predecessor.\n");
    conn_close(&node->pred);

    node_reset(node);
    printf("Nó abandonou o anel.\n");
}

/* ------------------------------------------------------------------ */
/*  Mensagens do protocolo de saída                                    */
/* ------------------------------------------------------------------ */

int handle_con(Node *node, int fd_type, int j, const char *ip, int port)
{
    int fd;

    if (fd_type != FD_TYPE_SUCC || !node->joined)
        return (-1);

    conn_close(&node->succ);
    if (j == node->node_id)
    {
        /* Só resto eu: sem sessões TCP, sou predecessor e sucessor de mim próprio */
        conn_close(&node->pred);
        node->pred_id = node->node_id;
        node->succ_id = node->node_id;
        strncpy(node->pred_ip, node->ip, sizeof(node->pred_ip) - 1);
        strncpy(node->succ_ip, node->ip, sizeof(node->succ_ip) - 1);
        node->pred_tcp = node->ring_port;
        node->succ_tcp = node->ring_port;
        printf("Fiquei sozinho no anel %d.\n", node->ring_id);
        return (0);
    }

    fd = tcp_connect(ip, port);
    if (fd < 0 || send_line(fd, "NEW %d %s %d", node->node_id, node->ip, node->ring_port) < 0)
    {
        printf("Erro: não foi possível ligar ao novo sucessor %d (%s:%d). Anel interrompido.\n", j, ip, port);
        if (fd >= 0)
            close(fd);
        node->succ_id = UNKNOWN_ID;
        return (0);
    }
    node->succ.fd = fd;
    node->succ.len = 0;
    node->succ_id = j;
    strncpy(node->succ_ip, ip, sizeof(node->succ_ip) - 1);
    node->succ_ip[sizeof(node->succ_ip) - 1] = '\0';
    node->succ_tcp = port;
    printf("Novo sucessor: %d (%s:%d).\n", j, ip, port);
    return (0);
}

int handle_boot(Node *node)
{
    if (!node->joined)
        return (-1);
    node->is_boot = 1;
    printf("Passei a ser o nó de arranque do anel %d.\n", node->ring_id);
    return (0);
}
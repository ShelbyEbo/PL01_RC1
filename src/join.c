#include "../headers/header.h"

void join_empty(Node *node, int ring_id, int node_id)
{
    if (bootstrap_register(node, ring_id, node_id, node->ip, node->ring_port) == 0)
    {
        node->node_id = node_id;
        node->ring_id = ring_id;
        node->pred_id = node_id;
        node->succ_id = node_id;

        strncpy(node->pred_ip, node->ip, sizeof(node->pred_ip) - 1);
        node->pred_ip[sizeof(node->pred_ip) - 1] = '\0';
        strncpy(node->succ_ip, node->ip, sizeof(node->succ_ip) - 1);
        node->succ_ip[sizeof(node->succ_ip) - 1] = '\0';

        node->pred_tcp = node->ring_port;
        node->succ_tcp = node->ring_port;

        node->joined = 1;
        node->is_boot = 1;

        printf("Anel %d criado. Nó %d registado como nó de arranque em (%s:%d).\n", ring_id, node_id, node->ip, node->ring_port);
    }
    else
    {
        printf("Erro ao registar o nó no Bootstrap Server.\n");
    }
}

static int join_link(Node *node, int ring_id, int node_id, int succ_id, const char *succ_ip, int succ_tcp)
{
    int fd;

    fd = tcp_connect(succ_ip, succ_tcp);
    if (fd < 0)
    {
        printf("Erro: não foi possível estabelecer sessão TCP com o sucessor %d (%s:%d).\n", succ_id, succ_ip, succ_tcp);
        return (-1);
    }
    if (send_line(fd, "NEW %d %s %d", node_id, node->ip, node->ring_port) < 0)
    {
        printf("Erro: falha ao enviar NEW ao sucessor.\n");
        close(fd);
        return (-1);
    }

    node->succ.fd = fd;
    node->succ.len = 0;
    node->ring_id = ring_id;
    node->node_id = node_id;
    node->succ_id = succ_id;
    strncpy(node->succ_ip, succ_ip, sizeof(node->succ_ip) - 1);
    node->succ_ip[sizeof(node->succ_ip) - 1] = '\0';
    node->succ_tcp = succ_tcp;
    node->pred_id = UNKNOWN_ID;
    node->pred_ip[0] = '\0';
    node->pred_tcp = 0;
    node->joined = 1;
    node->is_boot = 0;
    return (0);
}

void join_existing(Node *node, BootResponse *resp, int node_id)
{
    char    line[BUFFER_SIZE];
    char    l_ip[16];
    int     l;
    int     l_tcp;
    int     fd;

    if (resp->boot_node_id == node_id)
    {
        printf("Erro: O identificador %d já está em uso como nó de arranque. Escolha outro identificador.\n", node_id);
        return ;
    }
    printf("Anel %d existente. Nó de arranque: %d em (%s:%d).\n", resp->ring_id, resp->boot_node_id, resp->boot_ip, resp->boot_tcp);

    fd = tcp_connect(resp->boot_ip, resp->boot_tcp);
    if (fd < 0)
    {
        printf("Erro: não foi possível ligar ao nó de arranque.\n");
        return ;
    }
    if (send_line(fd, "ID %d", node_id) < 0
        || read_line_timeout(fd, line, sizeof(line), JOIN_TIMEOUT_SEC) < 0)
    {
        printf("Erro: o nó de arranque não respondeu ao pedido de adesão.\n");
        close(fd);
        return ;
    }
    close(fd);

    if (sscanf(line, "SUCC %d %15s %d", &l, l_ip, &l_tcp) != 3
        || !valid_id(l) || !valid_ip(l_ip) || !valid_port(l_tcp))
    {
        printf("Erro: resposta inválida do nó de arranque (%s).\n", line);
        return ;
    }
    if (l == node_id)
    {
        printf("Erro: o identificador %d já está em uso no anel. Escolha outro identificador.\n", node_id);
        return ;
    }
    if (join_link(node, resp->ring_id, node_id, l, l_ip, l_tcp) == 0)
        printf("Nó %d juntou-se ao anel %d. Sucessor: %d (%s:%d).\n", node_id, resp->ring_id, l, l_ip, l_tcp);
}

void join_direct(Node *node, int ring_id, int node_id, int succ_id, const char *succ_ip, int succ_tcp)
{
    if (!valid_id(succ_id) || succ_id == node_id || !valid_ip(succ_ip) || !valid_port(succ_tcp))
    {
        printf("Erro: sucessor inválido (id entre 0 e %d, diferente do próprio, IP e porta válidos).\n", RING_SIZE - 1);
        return ;
    }
    if (join_link(node, ring_id, node_id, succ_id, succ_ip, succ_tcp) == 0)
        printf("Nó %d juntou-se ao anel %d. Sucessor: %d (%s:%d).\n", node_id, ring_id, succ_id, succ_ip, succ_tcp);
}

/* ------------------------------------------------------------------ */
/*  Mensagens do protocolo de adesão                                   */
/* ------------------------------------------------------------------ */

int handle_new(Node *node, Conn *c, int fd_type, int j, const char *ip, int port)
{
    int fd;

    if (fd_type != FD_TYPE_TEMP || !node->joined || j == node->node_id)
        return (-1);

    if (node->succ_id == node->node_id)
    {
        /* Estava sozinho: j passa a ser predecessor e sucessor */
        node->pred = *c;
        conn_init(c);
        node->pred_id = j;
        strncpy(node->pred_ip, ip, sizeof(node->pred_ip) - 1);
        node->pred_ip[sizeof(node->pred_ip) - 1] = '\0';
        node->pred_tcp = port;

        fd = tcp_connect(ip, port);
        if (fd < 0 || send_line(fd, "NEW %d %s %d", node->node_id, node->ip, node->ring_port) < 0)
        {
            printf("Erro: não foi possível ligar ao novo nó %d. A voltar a ficar sozinho.\n", j);
            if (fd >= 0)
                close(fd);
            conn_close(&node->pred);
            node->pred_id = node->node_id;
            strncpy(node->pred_ip, node->ip, sizeof(node->pred_ip) - 1);
            node->pred_tcp = node->ring_port;
            return (0);
        }
        node->succ.fd = fd;
        node->succ.len = 0;
        node->succ_id = j;
        strncpy(node->succ_ip, ip, sizeof(node->succ_ip) - 1);
        node->succ_ip[sizeof(node->succ_ip) - 1] = '\0';
        node->succ_tcp = port;
        printf("Novo nó %d (%s:%d) é agora o meu predecessor e sucessor.\n", j, ip, port);
        conn_drain(node, &node->pred, FD_TYPE_PRED);
        return (0);
    }

    if (node->pred.fd >= 0)
    {
        /* Avisar o antigo predecessor m para se ligar a j */
        send_line(node->pred.fd, "CON %d %s %d", j, ip, port);
        conn_close(&node->pred);
    }
    node->pred = *c;
    conn_init(c);
    node->pred_id = j;
    strncpy(node->pred_ip, ip, sizeof(node->pred_ip) - 1);
    node->pred_ip[sizeof(node->pred_ip) - 1] = '\0';
    node->pred_tcp = port;
    printf("Novo predecessor: %d (%s:%d).\n", j, ip, port);
    conn_drain(node, &node->pred, FD_TYPE_PRED);
    return (0);
}

int handle_id(Node *node, Conn *c, int fd_type, int k)
{
    if (fd_type != FD_TYPE_TEMP || !node->joined)
        return (-1);
    if (pending_add(node, PEND_JOIN, k, c->fd) < 0)
    {
        printf("Aviso: demasiados pedidos de adesão em curso.\n");
        return (-1);
    }
    search_lookup(node, k);
    return (0);
}
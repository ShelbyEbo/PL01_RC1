#include "../headers/header.h"

/**
 * @brief Distância no sentido dos ponteiros do relógio de from até to (anel de RING_SIZE ids).
 */
int ring_dist(int from, int to)
{
    return (((to - from) % RING_SIZE + RING_SIZE) % RING_SIZE);
}

/**
 * @brief Verifica se este nó é responsável pelo identificador k, ou seja,
 *        se k está em (pred, self] no anel.
 */
int is_responsible(Node *node, int k)
{
    int dist_k;
    int dist_self;

    if (node->pred_id == node->node_id)     /* único nó: responsável por tudo */
        return (1);
    if (node->pred_id == UNKNOWN_ID)        /* anel em reparação: não responde */
        return (0);
    dist_k = ring_dist(node->pred_id, k);
    dist_self = ring_dist(node->pred_id, node->node_id);
    return (dist_k != 0 && dist_k <= dist_self);
}

/* ------------------------------------------------------------------ */
/*  Pedidos pendentes (à espera do RSP que volta pelo anel)            */
/* ------------------------------------------------------------------ */

int pending_add(Node *node, int type, int key, int fd)
{
    int i;

    for (i = 0; i < MAX_PENDING; i++)
    {
        if (node->pending[i].type == PEND_NONE)
        {
            node->pending[i].type = type;
            node->pending[i].key = key;
            node->pending[i].fd = fd;
            return (0);
        }
    }
    return (-1);
}

/* Remove os pedidos que dependiam de uma sessão que acabou de fechar. */
void pending_drop_fd(Node *node, int fd)
{
    int i;

    if (fd < 0)
        return ;
    for (i = 0; i < MAX_PENDING; i++)
    {
        if (node->pending[i].type == PEND_JOIN && node->pending[i].fd == fd)
            node->pending[i].type = PEND_NONE;
    }
}

void pending_clear(Node *node)
{
    memset(node->pending, 0, sizeof(node->pending));
}

/**
 * @brief Chega a resposta (l, l.IP, l.TCP) para o identificador k:
 *        entrega-a a quem a pediu (utilizador ou nó que está a aderir).
 */
void search_resolve(Node *node, int k, int l, const char *l_ip, int l_tcp)
{
    Pending *p;
    int     i;

    for (i = 0; i < MAX_PENDING; i++)
    {
        p = &node->pending[i];
        if (p->type == PEND_NONE || p->key != k)
            continue ;
        if (p->type == PEND_SEARCH)
            printf("Nó responsável por %d: (%d, %s, %d)\n", k, l, l_ip, l_tcp);
        else if (p->type == PEND_JOIN && send_line(p->fd, "SUCC %d %s %d", l, l_ip, l_tcp) < 0)
            printf("Aviso: não foi possível enviar SUCC ao nó que pretende aderir.\n");
        p->type = PEND_NONE;
    }
}

/**
 * @brief Inicia a pesquisa de k a partir deste nó: se for responsável
 *        resolve logo, senão envia <QRY self k> ao sucessor.
 */
void search_lookup(Node *node, int k)
{
    if (is_responsible(node, k))
        search_resolve(node, k, node->node_id, node->ip, node->ring_port);
    else if (send_line(node->succ.fd, "QRY %d %d", node->node_id, k) < 0)
        printf("Erro: não foi possível enviar QRY ao sucessor.\n");
}

/**
 * @brief Comando "search k" do utilizador.
 */
void search_start(Node *node, int k)
{
    int i;

    if (!node->joined)
    {
        printf("Erro: O nó não pertence a nenhum anel.\n");
        return ;
    }
    if (!valid_id(k))
    {
        printf("Erro: o identificador deve estar entre 0 e %d.\n", RING_SIZE - 1);
        return ;
    }
    if (is_responsible(node, k))
    {
        printf("Nó responsável por %d: (%d, %s, %d)\n", k, node->node_id, node->ip, node->ring_port);
        return ;
    }
    if (node->succ.fd < 0)
    {
        printf("Erro: sem sessão TCP com o sucessor.\n");
        return ;
    }
    if (pending_add(node, PEND_SEARCH, k, -1) < 0)
    {
        printf("Erro: demasiadas pesquisas em curso.\n");
        return ;
    }
    if (send_line(node->succ.fd, "QRY %d %d", node->node_id, k) < 0)
    {
        printf("Erro: não foi possível enviar QRY ao sucessor.\n");
        for (i = 0; i < MAX_PENDING; i++)
        {
            if (node->pending[i].type == PEND_SEARCH && node->pending[i].key == k)
                node->pending[i].type = PEND_NONE;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Mensagens do protocolo de pesquisa                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief <QRY j k>: se sou responsável por k respondo ao predecessor com
 *        <RSP j k i i.IP i.TCP>; senão passo <QRY j k> ao sucessor.
 * @return 0 em sucesso, -1 se a mensagem não faz sentido neste estado
 */
int handle_qry(Node *node, int j, int k)
{
    if (!node->joined)
        return (-1);
    if (j == node->node_id)
    {
        printf("Aviso: o QRY %d %d voltou ao originador sem resposta (anel inconsistente).\n", j, k);
        return (0);
    }
    if (is_responsible(node, k))
    {
        if (send_line(node->pred.fd, "RSP %d %d %d %s %d", j, k, node->node_id, node->ip, node->ring_port) < 0)
            printf("Aviso: sem sessão TCP com o predecessor para enviar RSP.\n");
    }
    else if (send_line(node->succ.fd, "QRY %d %d", j, k) < 0)
        printf("Aviso: sem sessão TCP com o sucessor para encaminhar QRY.\n");
    return (0);
}

/**
 * @brief <RSP j k l l.IP l.TCP>: se j sou eu, a pesquisa terminou;
 *        senão passo a resposta ao predecessor.
 */
int handle_rsp(Node *node, int j, int k, int l, const char *l_ip, int l_tcp)
{
    if (!node->joined)
        return (-1);
    if (j == node->node_id)
        search_resolve(node, k, l, l_ip, l_tcp);
    else if (send_line(node->pred.fd, "RSP %d %d %d %s %d", j, k, l, l_ip, l_tcp) < 0)
        printf("Aviso: sem sessão TCP com o predecessor para encaminhar RSP.\n");
    return (0);
}
#include "../headers/header.h"

void    join_existing(BootResponse resp, int node_id)
{
    if (resp.boot_node_id == node_id)
        printf("Erro: O identificador %d já está em uso como nó de arranque. Escolha outro identificador.\n", node_id);
    else
    {
        printf("Anel %d existente. Nó de arranque: %d em (%s:%d).\n", resp.ring_id, resp.boot_node_id, resp.boot_ip, resp.boot_tcp);
        printf("Adesão com múltiplos nós requer ligação TCP ao nó de arranque (passo seguinte).\n");
    }
}

void    join_empty(Node *node, int ring_id, int node_id)
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
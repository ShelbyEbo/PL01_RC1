#include "../headers/header.h"

void leave_multiple(Node *node)
{
    (void)node;
}

void leave_alone(Node *node)
{
    if (node->is_boot)
    {
        if (bootstrap_unregister(node, node->ring_id) == 0)
            printf("Anel %d anulado no Bootstrap Server com sucesso.\n",
                node->ring_id);
        else
            printf("Aviso: Falha ao anular registo no Bootstrap Server.\n");
    }
    node->joined = 0;
    node->ring_id = 0;
    node->node_id = 0;
    node->is_boot = 0;
    printf("Nó abandonou o anel.\n");
}

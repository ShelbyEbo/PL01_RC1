#include "../headers/header.h"

void process_command(Node *node, char *command)
{
    char cmd[20];
    int ring_id;
    int node_id;
    BootResponse resp;

    if (sscanf(command, "%19s", cmd) != 1)
        return ;

    if (!strcmp(cmd, "join"))
    {
        if (node->joined)
        {
            printf("Erro: O nó já pertence ao anel %d.\n", node->ring_id);
            return ;
        }

        if (sscanf(command, "%19s %d %d", cmd, &ring_id, &node_id) == 3)
        {
            /* 1. Inquirir o Bootstrap Server */
            if (bootstrap_query(node, ring_id, &resp) < 0)
            {
                printf("Erro: Falha na comunicação com o Bootstrap Server (%s:%d).\n",
                       node->boot_ip, node->boot_port);
                return ;
            }

            /* Se o anel estiver vazio, este nó assume-se como nó de arranque */
            if (resp.is_empty)
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

                    printf("Anel %d criado. Nó %d registado como nó de arranque em (%s:%d).\n",
                           ring_id, node_id, node->ip, node->ring_port);
                }
                else
                {
                    printf("Erro ao registar o nó no Bootstrap Server.\n");
                }
            }
            else
            {
                /* O anel já existe */
                if (resp.boot_node_id == node_id)
                {
                    printf("Erro: O identificador %d já está em uso como nó de arranque. Escolha outro identificador.\n",
                           node_id);
                }
                else
                {
                    printf("Anel %d existente. Nó de arranque: %d em (%s:%d).\n",
                           resp.ring_id, resp.boot_node_id, resp.boot_ip, resp.boot_tcp);
                    printf("Adesão com múltiplos nós requer ligação TCP ao nó de arranque (passo seguinte).\n");
                }
            }
        }
        else
        {
            printf("Uso: join <ring_id> <node_id>\n");
        }
    }
    else if (!strcmp(cmd, "show"))
    {
        if (!node->joined)
        {
            printf("O nó não pertence a nenhum anel.\n");
            printf("Bootstrap Server: %s:%d\n", node->boot_ip, node->boot_port);
            printf("Servidor TCP: %s:%d\n", node->ip, node->ring_port);
        }
        else
        {
            printf("Ring: %d\n", node->ring_id);
            printf("Node: %d%s\n", node->node_id, node->is_boot ? " (Bootstrap Node)" : "");
            printf("Predecessor: %d (%s:%d)\n", node->pred_id, node->pred_ip, node->pred_tcp);
            printf("Successor: %d (%s:%d)\n", node->succ_id, node->succ_ip, node->succ_tcp);
        }
    }
    else if (!strcmp(cmd, "leave"))
    {
        if (!node->joined)
        {
            printf("Erro: O nó não pertence a nenhum anel.\n");
            return ;
        }

        /* Caso em que é o único nó no anel */
        if (node->pred_id == node->node_id && node->succ_id == node->node_id)
        {
            if (node->is_boot)
            {
                if (bootstrap_unregister(node, node->ring_id) == 0)
                {
                    printf("Anel %d anulado no Bootstrap Server com sucesso.\n", node->ring_id);
                }
                else
                {
                    printf("Aviso: Falha ao anular registo no Bootstrap Server.\n");
                }
            }
            node->joined = 0;
            node->ring_id = 0;
            node->node_id = 0;
            node->is_boot = 0;
            printf("Nó abandonou o anel.\n");
        }
        else
        {
            printf("Saída com múltiplos nós a implementar em leave.c.\n");
        }
    }
    else if (!strcmp(cmd, "search"))
    {
        printf("Comando search a implementar em search.c.\n");
    }
    else if (!strcmp(cmd, "exit"))
    {
        if (node->joined && node->is_boot && node->pred_id == node->node_id)
        {
            bootstrap_unregister(node, node->ring_id);
        }
        close_tcp_server(node);
        exit(0);
    }
    else
    {
        printf("Unknown command: %s\n", cmd);
    }
}

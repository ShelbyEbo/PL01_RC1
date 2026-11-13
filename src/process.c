#include "../headers/header.h"

static void handle_join_cmd(Node *node, char *command)
{
    char cmd[20];
    int ring_id;
    int node_id;
    BootResponse resp;

    if (node->joined)
    {
        printf("Erro: O nó já pertence ao anel %d.\n", node->ring_id);
        return ;
    }
    if (sscanf(command, "%19s %d %d", cmd, &ring_id, &node_id) != 3)
    {
        printf("Uso: join <ring_id> <node_id>\n");
        return ;
    }
    if (bootstrap_query(node, ring_id, &resp) < 0)
    {
        printf("Erro: Falha na comunicação com o Bootstrap Server (%s:%d).\n",
            node->boot_ip, node->boot_port);
        return ;
    }
    if (resp.is_empty)
        join_empty(node, ring_id, node_id);
    else
        join_existing(resp, node_id);
}

static void handle_show_cmd(Node *node)
{
    if (!node->joined)
    {
        printf("O nó não pertence a nenhum anel.\n");
        printf("Bootstrap Server: %s:%d\n", node->boot_ip, node->boot_port);
        printf("Servidor TCP: %s:%d\n", node->ip, node->ring_port);
        return ;
    }
    printf("Ring: %d\n", node->ring_id);
    printf("Node: %d%s\n", node->node_id,
        node->is_boot ? " (Bootstrap Node)" : "");
    printf("Predecessor: %d (%s:%d)\n",
        node->pred_id, node->pred_ip, node->pred_tcp);
    printf("Successor: %d (%s:%d)\n",
        node->succ_id, node->succ_ip, node->succ_tcp);
}

static void handle_leave_cmd(Node *node)
{
    if (!node->joined)
    {
        printf("Erro: O nó não pertence a nenhum anel.\n");
        return ;
    }
    if (node->pred_id == node->node_id && node->succ_id == node->node_id)
        leave_alone(node);
    else
        leave_multiple(node);
}

static void handle_exit_cmd(Node *node)
{
    if (node->joined && node->is_boot && node->pred_id == node->node_id)
        bootstrap_unregister(node, node->ring_id);
    close_tcp_server(node);
    exit(0);
}

void process_command(Node *node, char *command)
{
    char cmd[20];

    if (sscanf(command, "%19s", cmd) != 1)
        return ;
    if (!strcmp(cmd, "join"))
        handle_join_cmd(node, command);
    else if (!strcmp(cmd, "show"))
        handle_show_cmd(node);
    else if (!strcmp(cmd, "leave"))
        handle_leave_cmd(node);
    else if (!strcmp(cmd, "search"))
        printf("Comando search a implementar em search.c.\n");
    else if (!strcmp(cmd, "exit"))
        handle_exit_cmd(node);
    else
        printf("Unknown command: %s\n", cmd);
}

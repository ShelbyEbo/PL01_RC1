#include "../headers/header.h"

void process_command(Node *node, char *command)
{
    char cmd[20];
    int ring_id;
    int node_id;

    if (sscanf(command, "%19s %d %d", cmd, &ring_id, &node_id) >= 1)
    {
        if (!strcmp(cmd, "join"))
        {
            if (sscanf(command, "%19s %d %d", cmd, &ring_id, &node_id) == 3)
            {
                node->node_id = node_id;
                node->ring_id = ring_id;

                node->pred_id = node_id;
                node->succ_id = node_id;

                node->joined = 1;
            }
            else
            {
                printf("Usage: join <ring_id> <node_id>\n");
            }
        }
        else if (!strcmp(cmd, "show"))
        {
            printf("Ring: %d\n", node->ring_id);
            printf("Node: %d\n", node->node_id);
            printf("Predecessor: %d (%s:%d)\n", node->pred_id, node->pred_ip, node->pred_tcp);
            printf("Successor: %d (%s:%d)\n", node->succ_id, node->succ_ip, node->succ_tcp);
        }
        else if (!strcmp(cmd, "leave"))
        {
            // Implementar
        }
        else if (!strcmp(cmd, "search"))
        {
            
        }
        else if (!strcmp(cmd, "exit"))
        {
            return ;
        }
        else
        {
            printf("Unknown command: %s\n", cmd);
        }
    }
}

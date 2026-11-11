# ifndef HEADER_H
# define HEADER_H

# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
# include <string.h>
# include <sys/socket.h>
# include <netinet/in.h>


typedef struct node
{
    int ring_id;
    int node_id;

    int pred_id;
    int succ_id;

    char pred_ip[16];
    char succ_ip[16];

    int pred_tcp;
    int succ_tcp;

    int joined;
} Node;

void process_command(Node *node, char *command);

#endif
#include "../headers/header.h"

static RingEntry *g_rings = NULL;

static RingEntry *find_ring(int ring_id)
{
    RingEntry *cur = g_rings;
    while (cur)
    {
        if (cur->ring_id == ring_id)
            return (cur);
        cur = cur->next;
    }
    return (NULL);
}

static void register_ring(int ring_id, int node_id, const char *ip, int tcp_port)
{
    RingEntry *entry = find_ring(ring_id);
    if (entry)
    {
        entry->boot_node_id = node_id;
        strncpy(entry->boot_ip, ip, sizeof(entry->boot_ip) - 1);
        entry->boot_ip[sizeof(entry->boot_ip) - 1] = '\0';
        entry->boot_tcp = tcp_port;
        return ;
    }

    entry = (RingEntry *)malloc(sizeof(RingEntry));
    if (!entry)
    {
        perror("malloc");
        return ;
    }
    entry->ring_id = ring_id;
    entry->boot_node_id = node_id;
    strncpy(entry->boot_ip, ip, sizeof(entry->boot_ip) - 1);
    entry->boot_ip[sizeof(entry->boot_ip) - 1] = '\0';
    entry->boot_tcp = tcp_port;
    entry->next = g_rings;
    g_rings = entry;
}

static int unregister_ring(int ring_id)
{
    RingEntry **cur = &g_rings;
    while (*cur)
    {
        if ((*cur)->ring_id == ring_id)
        {
            RingEntry *to_free = *cur;
            *cur = (*cur)->next;
            free(to_free);
            return (1);
        }
        cur = &((*cur)->next);
    }
    return (0);
}

int main(int argc, char **argv)
{
    int port = DEFAULT_BOOT_PORT;
    int opt;
    int sock;
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    char reply[BUFFER_SIZE];
    char cmd[32];
    ssize_t n;

    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Uso: %s [-p port]\n", argv[0]);
                return (EXIT_FAILURE);
        }
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("Erro ao criar socket UDP");
        return (EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Erro no bind do Bootstrap Server");
        close(sock);
        return (EXIT_FAILURE);
    }

    printf("Bootstrap Server activo na porta UDP %d...\n", port);

    while (1)
    {
        client_len = sizeof(client_addr);
        n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                     (struct sockaddr *)&client_addr, &client_len);
        if (n < 0)
        {
            perror("recvfrom");
            continue ;
        }
        buffer[n] = '\0';

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        if (sscanf(buffer, "%31s", cmd) != 1)
            continue ;

        if (strcmp(cmd, "BQRY") == 0)
        {
            int ring_id;
            if (sscanf(buffer, "BQRY %d", &ring_id) == 1)
            {
                RingEntry *entry = find_ring(ring_id);
                if (entry)
                {
                    snprintf(reply, sizeof(reply), "BRSP %d %d %s %d\n",
                             entry->ring_id, entry->boot_node_id, entry->boot_ip, entry->boot_tcp);
                }
                else
                {
                    snprintf(reply, sizeof(reply), "EMPTY\n");
                }
                printf("[%s:%d] BQRY %d -> %s", client_ip, client_port, ring_id, reply);
                sendto(sock, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, client_len);
            }
        }
        else if (strcmp(cmd, "REG") == 0)
        {
            int ring_id, node_id, tcp_port;
            char node_ip[16];
            if (sscanf(buffer, "REG %d %d %15s %d", &ring_id, &node_id, node_ip, &tcp_port) == 4)
            {
                register_ring(ring_id, node_id, node_ip, tcp_port);
                snprintf(reply, sizeof(reply), "OK\n");
                printf("[%s:%d] REG ring=%d node=%d (%s:%d) -> OK\n",
                       client_ip, client_port, ring_id, node_id, node_ip, tcp_port);
                sendto(sock, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, client_len);
            }
        }
        else if (strcmp(cmd, "UNR") == 0)
        {
            int ring_id;
            if (sscanf(buffer, "UNR %d", &ring_id) == 1)
            {
                unregister_ring(ring_id);
                snprintf(reply, sizeof(reply), "OK\n");
                printf("[%s:%d] UNR ring=%d -> OK\n", client_ip, client_port, ring_id);
                sendto(sock, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, client_len);
            }
        }
        else
        {
            printf("[%s:%d] Comando desconhecido: %s\n", client_ip, client_port, buffer);
        }
    }

    close(sock);
    return (0);
}

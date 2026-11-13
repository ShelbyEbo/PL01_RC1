#include "../headers/header.h"

static RingEntry *g_rings = NULL;

static RingEntry *find_ring(int ring_id)
{
    RingEntry *cur;

    cur = g_rings;
    while (cur)
    {
        if (cur->ring_id == ring_id)
            return (cur);
        cur = cur->next;
    }
    return (NULL);
}

static void update_ring_entry(RingEntry *entry, int node_id, const char *ip, int tcp)
{
    entry->boot_node_id = node_id;
    strncpy(entry->boot_ip, ip, sizeof(entry->boot_ip) - 1);
    entry->boot_ip[sizeof(entry->boot_ip) - 1] = '\0';
    entry->boot_tcp = tcp;
}

static void register_ring(int ring_id, int node_id, const char *ip, int tcp_port)
{
    RingEntry *entry;

    entry = find_ring(ring_id);
    if (entry)
    {
        update_ring_entry(entry, node_id, ip, tcp_port);
        return ;
    }
    entry = (RingEntry *)malloc(sizeof(RingEntry));
    if (!entry)
    {
        perror("malloc");
        return ;
    }
    entry->ring_id = ring_id;
    update_ring_entry(entry, node_id, ip, tcp_port);
    entry->next = g_rings;
    g_rings = entry;
}

static int unregister_ring(int ring_id)
{
    RingEntry **cur;
    RingEntry *to_free;

    cur = &g_rings;
    while (*cur)
    {
        if ((*cur)->ring_id == ring_id)
        {
            to_free = *cur;
            *cur = (*cur)->next;
            free(to_free);
            return (1);
        }
        cur = &((*cur)->next);
    }
    return (0);
}

static int parse_boot_port(int argc, char **argv)
{
    int port;
    int opt;

    port = DEFAULT_BOOT_PORT;
    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        if (opt == 'p')
            port = atoi(optarg);
        else
        {
            fprintf(stderr, "Uso: %s [-p port]\n", argv[0]);
            return (-1);
        }
    }
    return (port);
}

static int init_boot_udp_socket(int port)
{
    int sock;
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("Erro ao criar socket UDP");
        return (-1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Erro no bind do Bootstrap Server");
        close(sock);
        return (-1);
    }
    return (sock);
}

static void handle_bqry(int sock, struct sockaddr_in *cli, const char *buf)
{
    char reply[BUFFER_SIZE];
    char cli_ip[INET_ADDRSTRLEN];
    RingEntry *entry;
    int ring_id;

    if (sscanf(buf, "BQRY %d", &ring_id) != 1)
        return ;
    entry = find_ring(ring_id);
    if (entry)
        snprintf(reply, sizeof(reply), "BRSP %d %d %s %d\n",
            entry->ring_id, entry->boot_node_id,
            entry->boot_ip, entry->boot_tcp);
    else
        snprintf(reply, sizeof(reply), "EMPTY\n");
    inet_ntop(AF_INET, &cli->sin_addr, cli_ip, sizeof(cli_ip));
    printf("[%s:%d] BQRY %d -> %s",
        cli_ip, ntohs(cli->sin_port), ring_id, reply);
    sendto(sock, reply, strlen(reply), 0,
        (struct sockaddr *)cli, sizeof(*cli));
}

static void handle_reg(int sock, struct sockaddr_in *cli, const char *buf)
{
    char reply[BUFFER_SIZE];
    char cli_ip[INET_ADDRSTRLEN];
    char node_ip[16];
    int ring_id;
    int node_id;
    int tcp_port;

    if (sscanf(buf, "REG %d %d %15s %d",
            &ring_id, &node_id, node_ip, &tcp_port) != 4)
        return ;
    register_ring(ring_id, node_id, node_ip, tcp_port);
    snprintf(reply, sizeof(reply), "OK\n");
    inet_ntop(AF_INET, &cli->sin_addr, cli_ip, sizeof(cli_ip));
    printf("[%s:%d] REG ring=%d node=%d (%s:%d) -> OK\n",
        cli_ip, ntohs(cli->sin_port), ring_id, node_id, node_ip, tcp_port);
    sendto(sock, reply, strlen(reply), 0,
        (struct sockaddr *)cli, sizeof(*cli));
}

static void handle_unr(int sock, struct sockaddr_in *cli, const char *buf)
{
    char reply[BUFFER_SIZE];
    char cli_ip[INET_ADDRSTRLEN];
    int ring_id;

    if (sscanf(buf, "UNR %d", &ring_id) != 1)
        return ;
    unregister_ring(ring_id);
    snprintf(reply, sizeof(reply), "OK\n");
    inet_ntop(AF_INET, &cli->sin_addr, cli_ip, sizeof(cli_ip));
    printf("[%s:%d] UNR ring=%d -> OK\n",
        cli_ip, ntohs(cli->sin_port), ring_id);
    sendto(sock, reply, strlen(reply), 0,
        (struct sockaddr *)cli, sizeof(*cli));
}

static void process_boot_packet(int sock, struct sockaddr_in *cli, const char *buf)
{
    char cmd[32];
    char cli_ip[INET_ADDRSTRLEN];

    if (sscanf(buf, "%31s", cmd) != 1)
        return ;
    if (strcmp(cmd, "BQRY") == 0)
        handle_bqry(sock, cli, buf);
    else if (strcmp(cmd, "REG") == 0)
        handle_reg(sock, cli, buf);
    else if (strcmp(cmd, "UNR") == 0)
        handle_unr(sock, cli, buf);
    else
    {
        inet_ntop(AF_INET, &cli->sin_addr, cli_ip, sizeof(cli_ip));
        printf("[%s:%d] Comando desconhecido: %s\n",
            cli_ip, ntohs(cli->sin_port), buf);
    }
}

int main(int argc, char **argv)
{
    int port;
    int sock;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    ssize_t n;

    port = parse_boot_port(argc, argv);
    if (port < 0)
        return (EXIT_FAILURE);
    sock = init_boot_udp_socket(port);
    if (sock < 0)
        return (EXIT_FAILURE);
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
        process_boot_packet(sock, &client_addr, buffer);
    }
    close(sock);
    return (0);
}
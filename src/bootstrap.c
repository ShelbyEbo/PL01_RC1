#include "../headers/header.h"

static int init_udp_dest(const char *ip, int port, struct sockaddr_in *addr)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (!ip || inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
        return (-1);
    return (0);
}

static int wait_udp_reply(int fd, Node *node)
{
    fd_set readfds;
    struct timeval tv;
    int ret;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec = BOOT_TIMEOUT_SEC;
    tv.tv_usec = 0;
    ret = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0)
    {
        perror("Erro no select()");
        return (-1);
    }
    if (ret == 0)
    {
        fprintf(stderr,
            "Erro: Timeout ao aguardar resposta do Bootstrap Server (%s:%d)\n",
            node->boot_ip, node->boot_port);
        return (-1);
    }
    return (0);
}

static int parse_brsp(const char *reply, BootResponse *res)
{
    int parsed_ring;
    int parsed_node;
    char parsed_ip[16];
    int parsed_tcp;

    if (sscanf(reply, "BRSP %d %d %15s %d",
            &parsed_ring, &parsed_node, parsed_ip, &parsed_tcp) != 4)
    {
        fprintf(stderr, "Erro: Resposta BRSP mal formatada: %s\n", reply);
        return (-1);
    }
    res->is_empty = 0;
    res->ring_id = parsed_ring;
    res->boot_node_id = parsed_node;
    strncpy(res->boot_ip, parsed_ip, sizeof(res->boot_ip) - 1);
    res->boot_ip[sizeof(res->boot_ip) - 1] = '\0';
    res->boot_tcp = parsed_tcp;
    return (0);
}

/**
 * @brief Obtém o endereço IP local utilizado na interface de rede para comunicar com o destino.
 */
int get_local_ip(const char *dest_ip, int dest_port, char *out_ip, size_t out_len)
{
    int sock;
    struct sockaddr_in serv;
    struct sockaddr_in local;
    socklen_t local_len;
    int port;

    if (!out_ip || out_len < 16)
        return (-1);
    strncpy(out_ip, "127.0.0.1", out_len - 1);
    out_ip[out_len - 1] = '\0';
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return (-1);
    port = dest_port > 0 ? dest_port : DEFAULT_BOOT_PORT;
    if (init_udp_dest(dest_ip, port, &serv) < 0
        || connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        close(sock);
        return (-1);
    }
    local_len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &local_len) < 0
        || inet_ntop(AF_INET, &local.sin_addr, out_ip, out_len) == NULL)
    {
        strncpy(out_ip, "127.0.0.1", out_len - 1);
        out_ip[out_len - 1] = '\0';
    }
    close(sock);
    return (0);
}

/**
 * @brief Envia uma mensagem UDP para o servidor de arranque e aguarda a resposta com timeout (select).
 */
int bootstrap_send_recv(Node *node, const char *msg, char *reply, size_t reply_len)
{
    int fd;
    struct sockaddr_in serv_addr;
    struct sockaddr_in from_addr;
    socklen_t from_len;
    ssize_t n;

    if (!node || !msg || !reply || reply_len == 0)
        return (-1);
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        perror("Erro ao criar socket UDP");
        return (-1);
    }
    if (init_udp_dest(node->boot_ip, node->boot_port, &serv_addr) < 0)
    {
        fprintf(stderr, "Erro: Endereço IP do Bootstrap Server inválido (%s)\n",
            node->boot_ip);
        close(fd);
        return (-1);
    }
    if (sendto(fd, msg, strlen(msg), 0,
            (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Erro ao enviar mensagem para o Bootstrap Server");
        close(fd);
        return (-1);
    }
    if (wait_udp_reply(fd, node) < 0)
    {
        close(fd);
        return (-1);
    }
    from_len = sizeof(from_addr);
    n = recvfrom(fd, reply, reply_len - 1, 0,
            (struct sockaddr *)&from_addr, &from_len);
    if (n < 0)
        perror("Erro no recvfrom()");
    else
        reply[n] = '\0';
    close(fd);
    return (n < 0 ? -1 : (int)n);
}

/**
 * @brief Inquire o servidor de arranque acerca da existência de nós no anel ring_id (<BQRY x>).
 */
int bootstrap_query(Node *node, int ring_id, BootResponse *res)
{
    char msg[BUFFER_SIZE];
    char reply[BUFFER_SIZE];
    char tag[32];

    if (!node || !res)
        return (-1);
    memset(res, 0, sizeof(BootResponse));
    snprintf(msg, sizeof(msg), "BQRY %d\n", ring_id);
    if (bootstrap_send_recv(node, msg, reply, sizeof(reply)) < 0)
        return (-1);
    if (sscanf(reply, "%31s", tag) != 1)
    {
        fprintf(stderr, "Erro: Resposta vazia ou inválida do Bootstrap Server\n");
        return (-1);
    }
    if (strcmp(tag, "EMPTY") == 0)
    {
        res->is_empty = 1;
        res->ring_id = ring_id;
        return (0);
    }
    if (strcmp(tag, "BRSP") == 0)
        return (parse_brsp(reply, res));
    fprintf(stderr, "Erro: Mensagem desconhecida do Bootstrap Server: %s\n", reply);
    return (-1);
}

/**
 * @brief Regista ou actualiza o nó de arranque de um anel no Bootstrap Server (<REG x i i.IP i.TCP>).
 */
int bootstrap_register(Node *node, int ring_id, int node_id, const char *ip, int tcp_port)
{
    char msg[BUFFER_SIZE];
    char reply[BUFFER_SIZE];
    char tag[32];

    if (!node || !ip)
        return (-1);
    snprintf(msg, sizeof(msg), "REG %d %d %s %d\n",
        ring_id, node_id, ip, tcp_port);
    if (bootstrap_send_recv(node, msg, reply, sizeof(reply)) < 0)
        return (-1);
    if (sscanf(reply, "%31s", tag) == 1 && strcmp(tag, "OK") == 0)
        return (0);
    fprintf(stderr, "Erro: Registo no Bootstrap Server rejeitado: %s\n", reply);
    return (-1);
}

/**
 * @brief Cancela o registo do anel no Bootstrap Server quando o último nó sai (<UNR x>).
 */
int bootstrap_unregister(Node *node, int ring_id)
{
    char msg[BUFFER_SIZE];
    char reply[BUFFER_SIZE];
    char tag[32];

    if (!node)
        return (-1);
    snprintf(msg, sizeof(msg), "UNR %d\n", ring_id);
    if (bootstrap_send_recv(node, msg, reply, sizeof(reply)) < 0)
        return (-1);
    if (sscanf(reply, "%31s", tag) == 1 && strcmp(tag, "OK") == 0)
        return (0);
    fprintf(stderr,
        "Erro: Anulação do anel (UNR) no Bootstrap Server falhou: %s\n", reply);
    return (-1);
}

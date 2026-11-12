#include "../headers/header.h"

/**
 * @brief Obtém o endereço IP local utilizado na interface de rede para comunicar com o destino.
 *
 * Abre um socket UDP e associa-o (connect) ao destino.
 * Desta forma, a tabela de encaminhamento do kernel define o IP local adequado
 * sem necessidade de enviar pacotes pela rede.
 */
int get_local_ip(const char *dest_ip, int dest_port, char *out_ip, size_t out_len)
{
    int sock;
    struct sockaddr_in serv;
    struct sockaddr_in local;
    socklen_t local_len;

    if (!out_ip || out_len < 16)
        return (-1);

    strncpy(out_ip, "127.0.0.1", out_len - 1);
    out_ip[out_len - 1] = '\0';

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return (-1);

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(dest_port > 0 ? dest_port : DEFAULT_BOOT_PORT);

    if (!dest_ip || inet_pton(AF_INET, dest_ip, &serv.sin_addr) <= 0)
    {
        close(sock);
        return (-1);
    }

    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        close(sock);
        return (-1);
    }

    local_len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &local_len) < 0)
    {
        close(sock);
        return (-1);
    }

    if (inet_ntop(AF_INET, &local.sin_addr, out_ip, out_len) == NULL)
    {
        strncpy(out_ip, "127.0.0.1", out_len - 1);
        out_ip[out_len - 1] = '\0';
    }

    close(sock);
    return (0);
}

/**
 * @brief Envia uma mensagem UDP para o servidor de arranque e aguarda a resposta com timeout (select).
 *
 * @param node Estrutura com dados do nó e configuração do Bootstrap Server
 * @param msg Mensagem a enviar
 * @param reply Buffer onde a resposta será armazenada
 * @param reply_len Tamanho máximo do buffer de resposta
 * @return int Número de bytes lidos ou -1 em caso de erro/timeout
 */
int bootstrap_send_recv(Node *node, const char *msg, char *reply, size_t reply_len)
{
    int fd;
    struct sockaddr_in serv_addr;
    struct sockaddr_in from_addr;
    socklen_t from_len;
    fd_set readfds;
    struct timeval tv;
    ssize_t n;
    int ret;

    if (!node || !msg || !reply || reply_len == 0)
        return (-1);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        perror("Erro ao criar socket UDP");
        return (-1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(node->boot_port);
    if (inet_pton(AF_INET, node->boot_ip, &serv_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Erro: Endereço IP do Bootstrap Server inválido (%s)\n", node->boot_ip);
        close(fd);
        return (-1);
    }

    n = sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (n < 0)
    {
        perror("Erro ao enviar mensagem para o Bootstrap Server");
        close(fd);
        return (-1);
    }

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    tv.tv_sec = BOOT_TIMEOUT_SEC;
    tv.tv_usec = 0;

    ret = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0)
    {
        perror("Erro no select()");
        close(fd);
        return (-1);
    }
    else if (ret == 0)
    {
        fprintf(stderr, "Erro: Timeout ao aguardar resposta do Bootstrap Server (%s:%d)\n",
                node->boot_ip, node->boot_port);
        close(fd);
        return (-1);
    }

    from_len = sizeof(from_addr);
    n = recvfrom(fd, reply, reply_len - 1, 0, (struct sockaddr *)&from_addr, &from_len);
    if (n < 0)
    {
        perror("Erro no recvfrom()");
        close(fd);
        return (-1);
    }

    reply[n] = '\0';
    close(fd);
    return ((int)n);
}

/**
 * @brief Inquire o servidor de arranque acerca da existência de nós no anel ring_id (<BQRY x>).
 *
 * @param node Estrutura com dados do nó
 * @param ring_id Identificador do anel
 * @param res Estrutura onde será preenchida a resposta
 * @return int 0 em caso de sucesso, -1 em erro
 */
int bootstrap_query(Node *node, int ring_id, BootResponse *res)
{
    char msg[BUFFER_SIZE];
    char reply[BUFFER_SIZE];
    char tag[32];
    int parsed_ring;
    int parsed_node;
    char parsed_ip[16];
    int parsed_tcp;

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
    else if (strcmp(tag, "BRSP") == 0)
    {
        if (sscanf(reply, "BRSP %d %d %15s %d", &parsed_ring, &parsed_node, parsed_ip, &parsed_tcp) == 4)
        {
            res->is_empty = 0;
            res->ring_id = parsed_ring;
            res->boot_node_id = parsed_node;
            strncpy(res->boot_ip, parsed_ip, sizeof(res->boot_ip) - 1);
            res->boot_ip[sizeof(res->boot_ip) - 1] = '\0';
            res->boot_tcp = parsed_tcp;
            return (0);
        }
        else
        {
            fprintf(stderr, "Erro: Resposta BRSP mal formatada: %s\n", reply);
            return (-1);
        }
    }
    else
    {
        fprintf(stderr, "Erro: Mensagem desconhecida do Bootstrap Server: %s\n", reply);
        return (-1);
    }
}

/**
 * @brief Regista ou actualiza o nó de arranque de um anel no Bootstrap Server (<REG x i i.IP i.TCP>).
 *
 * @param node Estrutura com dados do nó
 * @param ring_id Identificador do anel
 * @param node_id Identificador do nó de arranque
 * @param ip Endereço IP do nó de arranque
 * @param tcp_port Porta TCP do servidor do nó
 * @return int 0 em caso de sucesso (resposta OK), -1 em caso de erro
 */
int bootstrap_register(Node *node, int ring_id, int node_id, const char *ip, int tcp_port)
{
    char msg[BUFFER_SIZE];
    char reply[BUFFER_SIZE];
    char tag[32];

    if (!node || !ip)
        return (-1);

    snprintf(msg, sizeof(msg), "REG %d %d %s %d\n", ring_id, node_id, ip, tcp_port);

    if (bootstrap_send_recv(node, msg, reply, sizeof(reply)) < 0)
        return (-1);

    if (sscanf(reply, "%31s", tag) == 1 && strcmp(tag, "OK") == 0)
        return (0);

    fprintf(stderr, "Erro: Registo no Bootstrap Server rejeitado: %s\n", reply);
    return (-1);
}

/**
 * @brief Cancela o registo do anel no Bootstrap Server quando o último nó sai (<UNR x>).
 *
 * @param node Estrutura com dados do nó
 * @param ring_id Identificador do anel a cancelar
 * @return int 0 em caso de sucesso (resposta OK), -1 em caso de erro
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

    fprintf(stderr, "Erro: Anulação do anel (UNR) no Bootstrap Server falhou: %s\n", reply);
    return (-1);
}

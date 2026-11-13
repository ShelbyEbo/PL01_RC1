#include "../headers/header.h"

static void reset_node_fds(Node *node)
{
    node->server_fd = -1;
    node->pred_fd = -1;
    node->succ_fd = -1;
    node->temp_fd = -1;
}

static int bind_tcp_socket(int fd, int *port)
{
    struct sockaddr_in addr;
    socklen_t addr_len;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(*port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Erro no bind() do servidor TCP");
        return (-1);
    }
    addr_len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) == 0)
        *port = ntohs(addr.sin_port);
    return (0);
}

/**
 * @brief Inicializa o servidor TCP do nó escutando na ringport.
 */
int init_tcp_server(Node *node)
{
    int opt;

    if (!node)
        return (-1);
    reset_node_fds(node);
    node->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->server_fd < 0)
    {
        perror("Erro ao criar socket TCP de escuta");
        return (-1);
    }
    opt = 1;
    if (setsockopt(node->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
            sizeof(opt)) < 0)
        perror("Aviso no setsockopt(SO_REUSEADDR)");
    if (bind_tcp_socket(node->server_fd, &node->ring_port) < 0
        || listen(node->server_fd, 5) < 0)
    {
        if (errno)
            perror("Erro no servidor TCP");
        close(node->server_fd);
        node->server_fd = -1;
        return (-1);
    }
    return (0);
}

/**
 * @brief Fecha todos os descritores de sockets TCP associados ao nó.
 */
void close_tcp_server(Node *node)
{
    if (!node)
        return ;
    if (node->server_fd >= 0)
        close(node->server_fd);
    if (node->pred_fd >= 0)
        close(node->pred_fd);
    if (node->succ_fd >= 0)
        close(node->succ_fd);
    if (node->temp_fd >= 0)
        close(node->temp_fd);
    reset_node_fds(node);
}

/**
 * @brief Aceita um novo pedido de conexão TCP e guarda no descritor temporário.
 */
int handle_new_connection(Node *node)
{
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int new_fd;

    if (!node || node->server_fd < 0)
        return (-1);
    client_len = sizeof(client_addr);
    new_fd = accept(node->server_fd,
            (struct sockaddr *)&client_addr, &client_len);
    if (new_fd < 0)
    {
        perror("Erro no accept()");
        return (-1);
    }
    if (node->temp_fd >= 0)
        close(node->temp_fd);
    node->temp_fd = new_fd;
    return (new_fd);
}

static int *get_target_fd(Node *node, int fd_type, const char **origin)
{
    if (fd_type == FD_TYPE_PRED)
    {
        *origin = "Predecessor";
        return (&node->pred_fd);
    }
    if (fd_type == FD_TYPE_SUCC)
    {
        *origin = "Sucessor";
        return (&node->succ_fd);
    }
    if (fd_type == FD_TYPE_TEMP)
    {
        *origin = "Cliente Temporário";
        return (&node->temp_fd);
    }
    return (NULL);
}

/**
 * @brief Trata mensagens recebidas num dos sockets TCP (predecessor, sucessor ou temporário).
 */
int handle_socket_message(Node *node, int fd_type)
{
    int *target_fd;
    const char *origin;
    char buffer[BUFFER_SIZE];
    ssize_t n;

    if (!node)
        return (-1);
    target_fd = get_target_fd(node, fd_type, &origin);
    if (!target_fd || *target_fd < 0)
        return (-1);
    n = read(*target_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0)
    {
        if (n == 0)
            printf("[%s] Conexão TCP encerrada pelo par remoto.\n", origin);
        else
            perror("Erro no read() da conexão TCP");
        close(*target_fd);
        *target_fd = -1;
        return (0);
    }
    buffer[n] = '\0';
    if (buffer[n - 1] == '\n')
        buffer[n - 1] = '\0';
    printf("[%s] Mensagem TCP recebida: %s\n", origin, buffer);
    return (1);
}

static void add_fd_to_set(int fd, fd_set *readfds, int *max_fd)
{
    if (fd < 0)
        return ;
    FD_SET(fd, readfds);
    if (fd > *max_fd)
        *max_fd = fd;
}

static int build_fd_set(Node *node, fd_set *readfds)
{
    int max_fd;

    FD_ZERO(readfds);
    FD_SET(STDIN_FILENO, readfds);
    max_fd = STDIN_FILENO;
    add_fd_to_set(node->server_fd, readfds, &max_fd);
    add_fd_to_set(node->pred_fd, readfds, &max_fd);
    add_fd_to_set(node->succ_fd, readfds, &max_fd);
    add_fd_to_set(node->temp_fd, readfds, &max_fd);
    return (max_fd);
}

static int dispatch_fd_events(Node *node, fd_set *readfds)
{
    char command[BUFFER_SIZE];

    if (FD_ISSET(STDIN_FILENO, readfds))
    {
        if (fgets(command, sizeof(command), stdin) == NULL)
            return (-1);
        process_command(node, command);
    }
    if (node->server_fd >= 0 && FD_ISSET(node->server_fd, readfds))
        handle_new_connection(node);
    if (node->pred_fd >= 0 && FD_ISSET(node->pred_fd, readfds))
        handle_socket_message(node, FD_TYPE_PRED);
    if (node->succ_fd >= 0 && FD_ISSET(node->succ_fd, readfds))
        handle_socket_message(node, FD_TYPE_SUCC);
    if (node->temp_fd >= 0 && FD_ISSET(node->temp_fd, readfds))
        handle_socket_message(node, FD_TYPE_TEMP);
    return (0);
}

/**
 * @brief Ciclo de eventos principal multiplexando STDIN e sockets TCP via select().
 */
void run_event_loop(Node *node)
{
    fd_set readfds;
    int max_fd;
    int ret;

    while (1)
    {
        max_fd = build_fd_set(node, &readfds);
        ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue ;
            perror("Erro no select()");
            break ;
        }
        if (dispatch_fd_events(node, &readfds) < 0)
            break ;
    }
}

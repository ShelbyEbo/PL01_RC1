#include "../headers/header.h"

/**
 * @brief Inicializa o servidor TCP do nó escutando na ringport.
 *
 * @param node Ponteiro para o nó
 * @return int 0 em sucesso, -1 em erro
 */
int init_tcp_server(Node *node)
{
    struct sockaddr_in addr;
    socklen_t addr_len;
    int opt;

    if (!node)
        return (-1);

    node->server_fd = -1;
    node->pred_fd = -1;
    node->succ_fd = -1;
    node->temp_fd = -1;

    node->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->server_fd < 0)
    {
        perror("Erro ao criar socket TCP de escuta");
        return (-1);
    }

    opt = 1;
    if (setsockopt(node->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Aviso no setsockopt(SO_REUSEADDR)");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(node->ring_port);

    if (bind(node->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Erro no bind() do servidor TCP");
        close(node->server_fd);
        node->server_fd = -1;
        return (-1);
    }

    /* Se a porta especificada foi 0, obter a porta efémera atribuída pelo sistema */
    addr_len = sizeof(addr);
    if (getsockname(node->server_fd, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        node->ring_port = ntohs(addr.sin_port);
    }

    if (listen(node->server_fd, 5) < 0)
    {
        perror("Erro no listen() do servidor TCP");
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
    {
        close(node->server_fd);
        node->server_fd = -1;
    }
    if (node->pred_fd >= 0)
    {
        close(node->pred_fd);
        node->pred_fd = -1;
    }
    if (node->succ_fd >= 0)
    {
        close(node->succ_fd);
        node->succ_fd = -1;
    }
    if (node->temp_fd >= 0)
    {
        close(node->temp_fd);
        node->temp_fd = -1;
    }
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
    new_fd = accept(node->server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (new_fd < 0)
    {
        perror("Erro no accept()");
        return (-1);
    }

    /* Se já existia uma conexão temporária aberta sem conclusão, fechá-la */
    if (node->temp_fd >= 0)
    {
        close(node->temp_fd);
        node->temp_fd = -1;
    }

    node->temp_fd = new_fd;
    return (new_fd);
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

    if (fd_type == FD_TYPE_PRED)
    {
        target_fd = &node->pred_fd;
        origin = "Predecessor";
    }
    else if (fd_type == FD_TYPE_SUCC)
    {
        target_fd = &node->succ_fd;
        origin = "Sucessor";
    }
    else if (fd_type == FD_TYPE_TEMP)
    {
        target_fd = &node->temp_fd;
        origin = "Cliente Temporário";
    }
    else
    {
        return (-1);
    }

    if (*target_fd < 0)
        return (-1);

    n = read(*target_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0)
    {
        /* Fecho ou erro na ligação TCP */
        if (n == 0)
            printf("[%s] Conexão TCP encerrada pelo par remoto.\n", origin);
        else
            perror("Erro no read() da conexão TCP");

        close(*target_fd);
        *target_fd = -1;
        return (0);
    }

    buffer[n] = '\0';
    /* Remover nova linha para logging limpo */
    if (buffer[n - 1] == '\n')
        buffer[n - 1] = '\0';

    printf("[%s] Mensagem TCP recebida: %s\n", origin, buffer);

    /*
     * Os comandos específicos (QRY, RSP, ID, NEW, CON, BOOT)
     * serão despachados aqui para search.c, join.c e leave.c
     */

    return (1);
}

/**
 * @brief Ciclo de eventos principal multiplexando STDIN e sockets TCP via select().
 */
void run_event_loop(Node *node)
{
    fd_set readfds;
    char command[BUFFER_SIZE];
    int max_fd;
    int ret;

    while (1)
    {
        FD_ZERO(&readfds);

        /* 1. Terminal do utilizador */
        FD_SET(STDIN_FILENO, &readfds);
        max_fd = STDIN_FILENO;

        /* 2. Socket de escuta do servidor TCP */
        if (node->server_fd >= 0)
        {
            FD_SET(node->server_fd, &readfds);
            if (node->server_fd > max_fd)
                max_fd = node->server_fd;
        }

        /* 3. Conexão TCP com o Predecessor */
        if (node->pred_fd >= 0)
        {
            FD_SET(node->pred_fd, &readfds);
            if (node->pred_fd > max_fd)
                max_fd = node->pred_fd;
        }

        /* 4. Conexão TCP com o Sucessor */
        if (node->succ_fd >= 0)
        {
            FD_SET(node->succ_fd, &readfds);
            if (node->succ_fd > max_fd)
                max_fd = node->succ_fd;
        }

        /* 5. Conexão TCP temporária (handshake) */
        if (node->temp_fd >= 0)
        {
            FD_SET(node->temp_fd, &readfds);
            if (node->temp_fd > max_fd)
                max_fd = node->temp_fd;
        }

        ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue ;
            perror("Erro no select()");
            break ;
        }

        /* Entrada de comando do utilizador */
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            if (fgets(command, sizeof(command), stdin) == NULL)
                break ;
            process_command(node, command);
        }

        /* Novo cliente a conectar ao servidor TCP */
        if (node->server_fd >= 0 && FD_ISSET(node->server_fd, &readfds))
        {
            handle_new_connection(node);
        }

        /* Dados recebidos do predecessor */
        if (node->pred_fd >= 0 && FD_ISSET(node->pred_fd, &readfds))
        {
            handle_socket_message(node, FD_TYPE_PRED);
        }

        /* Dados recebidos do sucessor */
        if (node->succ_fd >= 0 && FD_ISSET(node->succ_fd, &readfds))
        {
            handle_socket_message(node, FD_TYPE_SUCC);
        }

        /* Dados recebidos de ligação temporária */
        if (node->temp_fd >= 0 && FD_ISSET(node->temp_fd, &readfds))
        {
            handle_socket_message(node, FD_TYPE_TEMP);
        }
    }
}

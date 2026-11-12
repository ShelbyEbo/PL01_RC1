#include "../headers/header.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>

void node_init(Node *node)
{
    char hostname[256];
    struct addrinfo hints = {0};
    struct addrinfo *addresses;

    memset(node, 0, sizeof(*node));
    node->listen_fd = -1;
    node->pred_fd = -1;
    node->succ_fd = -1;
    node->running = 1;
    strcpy(node->ip, "127.0.0.1");
    strcpy(node->boot_ip, "192.168.56.21");
    node->boot_port = 58000;
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        node->connections[i].fd = -1;
        node->connections[i].pending_key = -1;
    }
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        hostname[sizeof(hostname) - 1] = '\0';
        if (getaddrinfo(hostname, NULL, &hints, &addresses) == 0)
        {
            struct sockaddr_in *address = (struct sockaddr_in *)addresses->ai_addr;
            inet_ntop(AF_INET, &address->sin_addr, node->ip, sizeof(node->ip));
            freeaddrinfo(addresses);
        }
    }
}

static int add_connection(Node *node, int fd)
{
    if (fd >= FD_SETSIZE)
    {
        close(fd);
        return -1;
    }
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (node->connections[i].fd == -1)
        {
            node->connections[i].fd = fd;
            node->connections[i].used = 0;
            node->connections[i].pending_key = -1;
            return fd;
        }
    }
    fprintf(stderr, "Limite de ligações TCP atingido.\n");
    close(fd);
    return -1;
}

int tcp_start(Node *node, int port)
{
    struct sockaddr_in address = {0};
    socklen_t length = sizeof(address);
    int reuse = 1;
    int fd;

    if (port < 0 || port > 65535 || node->listen_fd != -1)
        return -1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (fd >= FD_SETSIZE
        || setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1
        || bind(fd, (struct sockaddr *)&address, sizeof(address)) == -1
        || listen(fd, MAX_CONNECTIONS) == -1
        || getsockname(fd, (struct sockaddr *)&address, &length) == -1)
    {
        perror("servidor TCP");
        close(fd);
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    node->listen_fd = fd;
    node->tcp_port = ntohs(address.sin_port);
    return 0;
}

int tcp_connect(Node *node, const char *ip, int port)
{
    struct sockaddr_in address = {0};
    int fd;

    address.sin_family = AF_INET;
    if (port < 1 || port > 65535 || inet_pton(AF_INET, ip, &address.sin_addr) != 1)
        return -1;
    address.sin_port = htons(port);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("connect");
        close(fd);
        return -1;
    }
    return add_connection(node, fd);
}

void tcp_close(Node *node, int fd)
{
    if (fd < 0)
        return;
    if (fd == node->pred_fd || fd == node->succ_fd)
    {
        fprintf(stderr, "Ligação com vizinho encerrada; pesquisas pendentes canceladas.\n");
        memset(node->pending_search, 0, sizeof(node->pending_search));
        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (node->connections[i].pending_key >= 0)
                tcp_close(node, node->connections[i].fd);
        }
    }
    if (node->pred_fd == fd)
        node->pred_fd = -1;
    if (node->succ_fd == fd)
        node->succ_fd = -1;
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (node->connections[i].fd == fd)
        {
            close(fd);
            node->connections[i].fd = -1;
            node->connections[i].used = 0;
            node->connections[i].pending_key = -1;
            return;
        }
    }
}

int tcp_send(Node *node, int fd, const char *message)
{
    size_t sent = 0;
    size_t length = strlen(message);

    if (fd < 0)
        return -1;
    while (sent < length)
    {
        ssize_t count = write(fd, message + sent, length - sent);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
        {
            perror("write TCP");
            tcp_close(node, fd);
            return -1;
        }
        sent += (size_t)count;
    }
    return 0;
}


static void read_connection(Node *node, Connection *connection)
{
    char data[MESSAGE_SIZE];
    ssize_t count = read(connection->fd, data, sizeof(data));

    if (count < 0 && errno == EINTR)
        return;
    if (count <= 0)
    {
        if (count < 0)
            perror("read TCP");
        else if (connection->used != 0)
            fprintf(stderr, "Mensagem TCP incompleta.\n");
        tcp_close(node, connection->fd);
        return;
    }
    for (ssize_t i = 0; i < count; i++)
    {
        if (data[i] == '\n')
        {
            if (connection->used > 0 && connection->buffer[connection->used - 1] == '\r')
                connection->used--;
            connection->buffer[connection->used] = '\0';
            connection->used = 0;
            if (tcp_handle_message(node, connection->fd, connection->buffer) == -1)
            {
                fprintf(stderr, "Mensagem TCP inválida ou pesquisa indisponível.\n");
                tcp_close(node, connection->fd);
                return;
            }
            if (connection->fd == -1)
                return;
        }
        else if (data[i] == '\0' || connection->used == MESSAGE_SIZE - 1)
        {
            fprintf(stderr, "Mensagem TCP inválida ou demasiado longa.\n");
            tcp_close(node, connection->fd);
            return;
        }
        else
            connection->buffer[connection->used++] = data[i];
    }
}

int tcp_add_fds(Node *node, fd_set *read_fds)
{
    int max_fd = node->listen_fd;

    if (node->listen_fd >= 0)
        FD_SET(node->listen_fd, read_fds);
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        int fd = node->connections[i].fd;
        if (fd >= 0)
        {
            FD_SET(fd, read_fds);
            if (fd > max_fd)
                max_fd = fd;
        }
    }
    return max_fd;
}

void tcp_process_fds(Node *node, fd_set *read_fds)
{
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        int fd = node->connections[i].fd;
        if (fd >= 0 && FD_ISSET(fd, read_fds))
            read_connection(node, &node->connections[i]);
    }
    if (node->listen_fd >= 0 && FD_ISSET(node->listen_fd, read_fds))
    {
        int fd = accept(node->listen_fd, NULL, NULL);
        if (fd >= 0)
            add_connection(node, fd);
        else if (errno != EINTR)
            perror("accept");
    }
}

void tcp_stop(Node *node)
{
    node->pred_fd = -1;
    node->succ_fd = -1;
    for (int i = 0; i < MAX_CONNECTIONS; i++)
        tcp_close(node, node->connections[i].fd);
    if (node->listen_fd >= 0)
        close(node->listen_fd);
    node->listen_fd = -1;
}

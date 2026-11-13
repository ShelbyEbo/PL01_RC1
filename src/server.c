#include "../headers/header.h"

static Conn *connection_for_type(Node *node, int fd_type)
{
    if (fd_type == FD_TYPE_PRED)
        return (&node->pred);
    if (fd_type == FD_TYPE_SUCC)
        return (&node->succ);
    if (fd_type == FD_TYPE_TEMP)
        return (&node->temp[0]);
    return (NULL);
}

void conn_init(Conn *c)
{
    if (!c)
        return ;
    c->fd = -1;
    c->len = 0;
    c->buf[0] = '\0';
}

void conn_close(Conn *c)
{
    if (!c)
        return ;
    if (c->fd >= 0)
        close(c->fd);
    conn_init(c);
}

int valid_id(int id)
{
    return (id >= 0 && id < RING_SIZE);
}

int valid_port(int port)
{
    return (port > 0 && port <= 65535);
}

int valid_ip(const char *ip)
{
    struct in_addr addr;

    return (ip && inet_pton(AF_INET, ip, &addr) == 1);
}

int send_line(int fd, const char *fmt, ...)
{
    char    line[BUFFER_SIZE];
    va_list ap;
    size_t  len;
    size_t  sent;
    int     n;
    ssize_t written;

    if (fd < 0 || !fmt)
        return (-1);
    va_start(ap, fmt);
    n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n + 1 >= sizeof(line))
        return (-1);
    len = (size_t)n;
    line[len++] = '\n';
    sent = 0;
    while (sent < len)
    {
        written = send(fd, line + sent, len - sent, 0);
        if (written <= 0)
        {
            if (written < 0 && errno == EINTR)
                continue ;
            return (-1);
        }
        sent += (size_t)written;
    }
    return (0);
}

int tcp_connect(const char *ip, int port)
{
    struct sockaddr_in addr;
    int                 fd;

    if (!valid_ip(ip) || !valid_port(port))
        return (-1);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return (-1);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return (-1);
    }
    return (fd);
}

int read_line_timeout(int fd, char *out, size_t len, int seconds)
{
    fd_set          readfds;
    struct timeval  timeout;
    size_t          used;
    char            c;
    ssize_t         n;
    int             ret;

    if (fd < 0 || !out || len < 2)
        return (-1);
    used = 0;
    while (used + 1 < len)
    {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        timeout.tv_sec = seconds;
        timeout.tv_usec = 0;
        ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
        if (ret <= 0)
            return (-1);
        n = recv(fd, &c, 1, 0);
        if (n <= 0)
            return (-1);
        if (c == '\n')
            break ;
        if (c != '\r')
            out[used++] = c;
    }
    out[used] = '\0';
    return (0);
}

static int bind_tcp_socket(int fd, int *port)
{
    struct sockaddr_in addr;
    socklen_t          addr_len;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(*port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return (-1);
    addr_len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) == 0)
        *port = ntohs(addr.sin_port);
    return (0);
}

int init_tcp_server(Node *node)
{
    int i;
    int opt;

    if (!node)
        return (-1);
    conn_init(&node->pred);
    conn_init(&node->succ);
    for (i = 0; i < MAX_TEMP; i++)
        conn_init(&node->temp[i]);
    pending_clear(node);
    node->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->server_fd < 0)
        return (-1);
    opt = 1;
    setsockopt(node->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind_tcp_socket(node->server_fd, &node->ring_port) < 0
        || listen(node->server_fd, 5) < 0)
    {
        close(node->server_fd);
        node->server_fd = -1;
        return (-1);
    }
    return (0);
}

void close_tcp_server(Node *node)
{
    int i;

    if (!node)
        return ;
    if (node->server_fd >= 0)
        close(node->server_fd);
    node->server_fd = -1;
    conn_close(&node->pred);
    conn_close(&node->succ);
    for (i = 0; i < MAX_TEMP; i++)
        conn_close(&node->temp[i]);
    pending_clear(node);
}

int handle_new_connection(Node *node)
{
    struct sockaddr_in client_addr;
    socklen_t          client_len;
    int                fd;

    if (!node || node->server_fd < 0)
        return (-1);
    client_len = sizeof(client_addr);
    fd = accept(node->server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (fd < 0)
        return (-1);
    if (node->temp[0].fd >= 0)
    {
        pending_drop_fd(node, node->temp[0].fd);
        conn_close(&node->temp[0]);
    }
    node->temp[0].fd = fd;
    return (fd);
}

static void drop_connection(Node *node, Conn *c, int fd_type)
{
    int fd;

    fd = c->fd;
    conn_close(c);
    if (fd_type == FD_TYPE_TEMP)
        pending_drop_fd(node, fd);
}

static int dispatch_line(Node *node, Conn *c, int fd_type, char *line)
{
    char ip[16];
    char cmd[8];
    int  a;
    int  b;
    int  l;
    int  port;

    if (sscanf(line, "%7s", cmd) != 1)
        return (-1);
    if (!strcmp(cmd, "NEW") && sscanf(line, "NEW %d %15s %d", &a, ip, &port) == 3)
        return (handle_new(node, c, fd_type, a, ip, port));
    if (!strcmp(cmd, "ID") && sscanf(line, "ID %d", &a) == 1)
        return (handle_id(node, c, fd_type, a));
    if (!strcmp(cmd, "CON") && sscanf(line, "CON %d %15s %d", &a, ip, &port) == 3)
        return (handle_con(node, fd_type, a, ip, port));
    if (!strcmp(cmd, "BOOT"))
        return (!strcmp(line, "BOOT") ? handle_boot(node) : -1);
    if (!strcmp(cmd, "QRY") && sscanf(line, "QRY %d %d", &a, &b) == 2)
        return (handle_qry(node, a, b));
    if (!strcmp(cmd, "RSP")
        && sscanf(line, "RSP %d %d %d %15s %d", &a, &b, &l, ip, &port) == 5)
        return (handle_rsp(node, a, b, l, ip, port));
    return (-1);
}

void conn_drain(Node *node, Conn *c, int fd_type)
{
    char    line[BUFFER_SIZE];
    char   *newline;
    ssize_t n;
    size_t  consumed;
    int     result;

    if (!node || !c || c->fd < 0)
        return ;
    n = recv(c->fd, c->buf + c->len, sizeof(c->buf) - c->len - 1, 0);
    if (n <= 0)
    {
        drop_connection(node, c, fd_type);
        return ;
    }
    c->len += (size_t)n;
    c->buf[c->len] = '\0';
    while ((newline = strchr(c->buf, '\n')) != NULL)
    {
        consumed = (size_t)(newline - c->buf) + 1;
        if (consumed - 1 >= sizeof(line))
        {
            drop_connection(node, c, fd_type);
            return ;
        }
        memcpy(line, c->buf, consumed - 1);
        line[consumed - 1] = '\0';
        if (consumed > 1 && line[consumed - 2] == '\r')
            line[consumed - 2] = '\0';
        memmove(c->buf, c->buf + consumed, c->len - consumed);
        c->len -= consumed;
        c->buf[c->len] = '\0';
        result = dispatch_line(node, c, fd_type, line);
        if (result < 0)
        {
            drop_connection(node, c, fd_type);
            return ;
        }
        if (c->fd < 0)
            return ;
    }
}

int handle_socket_message(Node *node, int fd_type)
{
    Conn *c;

    c = connection_for_type(node, fd_type);
    if (!c || c->fd < 0)
        return (-1);
    conn_drain(node, c, fd_type);
    return (0);
}

static void add_fd(fd_set *set, int *max_fd, int fd)
{
    if (fd < 0)
        return ;
    FD_SET(fd, set);
    if (fd > *max_fd)
        *max_fd = fd;
}

void run_event_loop(Node *node)
{
    fd_set readfds;
    int    max_fd;
    int    ret;

    while (1)
    {
        FD_ZERO(&readfds);
        max_fd = STDIN_FILENO;
        FD_SET(STDIN_FILENO, &readfds);
        add_fd(&readfds, &max_fd, node->server_fd);
        add_fd(&readfds, &max_fd, node->pred.fd);
        add_fd(&readfds, &max_fd, node->succ.fd);
        add_fd(&readfds, &max_fd, node->temp[0].fd);
        ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue ;
            break ;
        }
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            char command[BUFFER_SIZE];

            if (!fgets(command, sizeof(command), stdin))
                break ;
            process_command(node, command);
        }
        if (node->server_fd >= 0 && FD_ISSET(node->server_fd, &readfds))
            handle_new_connection(node);
        if (node->pred.fd >= 0 && FD_ISSET(node->pred.fd, &readfds))
            handle_socket_message(node, FD_TYPE_PRED);
        if (node->succ.fd >= 0 && FD_ISSET(node->succ.fd, &readfds))
            handle_socket_message(node, FD_TYPE_SUCC);
        if (node->temp[0].fd >= 0 && FD_ISSET(node->temp[0].fd, &readfds))
            handle_socket_message(node, FD_TYPE_TEMP);
    }
}

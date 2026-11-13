#ifndef HEADER_H
# define HEADER_H

# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
# include <string.h>
# include <stdarg.h>
# include <signal.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <sys/select.h>
# include <sys/time.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <errno.h>

# define DEFAULT_BOOT_IP    "192.168.56.21"
# define DEFAULT_BOOT_PORT  58000
# define BUFFER_SIZE        256
# define BOOT_TIMEOUT_SEC   3
# define JOIN_TIMEOUT_SEC   5

/* h = 6  ->  64 identificadores em cada anel (0..63) */
# define RING_SIZE          64
# define UNKNOWN_ID         (-1)

# define MAX_TEMP           16  /* ligações aceites ainda sem papel definido */
# define MAX_PENDING        16  /* pesquisas / adesões à espera de RSP */

/* Identificadores do tipo de ligação TCP */
# define FD_TYPE_PRED 1
# define FD_TYPE_SUCC 2
# define FD_TYPE_TEMP 3

/* Tipos de pedidos pendentes */
# define PEND_NONE   0
# define PEND_SEARCH 1  /* pesquisa pedida pelo utilizador (search k) */
# define PEND_JOIN   2  /* pesquisa feita em nome de um nó que quer aderir (ID i) */

/* Uma sessão TCP com buffer próprio: o TCP é um fluxo de bytes, por isso
 * as mensagens (terminadas por '\n') têm de ser reconstruídas aqui. */
typedef struct conn
{
    int     fd;
    char    buf[BUFFER_SIZE];
    size_t  len;
} Conn;

typedef struct pending
{
    int type;
    int key;    /* identificador k que está a ser pesquisado */
    int fd;     /* PEND_JOIN: sessão do nó que espera o SUCC */
} Pending;

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

    /* Configuração local e do servidor de arranque (Bootstrap) */
    char boot_ip[16];
    int boot_port;
    char ip[16];
    int ring_port;
    int is_boot;

    /* Sockets TCP */
    int server_fd;              /* Socket de escuta TCP (ringport) */
    Conn pred;                  /* Sessão TCP com o predecessor */
    Conn succ;                  /* Sessão TCP com o sucessor */
    Conn temp[MAX_TEMP];        /* Ligações aceites ainda não classificadas */

    Pending pending[MAX_PENDING];
} Node;

typedef struct ring_entry
{
    int ring_id;
    int boot_node_id;
    char boot_ip[16];
    int boot_tcp;
    struct ring_entry *next;
} RingEntry;

typedef struct boot_response
{
    int is_empty;       /* 1 se o anel não tiver nós (EMPTY), 0 se tiver nós */
    int ring_id;
    int boot_node_id;
    char boot_ip[16];
    int boot_tcp;
} BootResponse;

/* process.c */
void process_command(Node *node, char *command);

/* bootstrap.c */
int get_local_ip(const char *dest_ip, int dest_port, char *out_ip, size_t out_len);
int bootstrap_send_recv(Node *node, const char *msg, char *reply, size_t reply_len);
int bootstrap_query(Node *node, int ring_id, BootResponse *res);
int bootstrap_register(Node *node, int ring_id, int node_id, const char *ip, int tcp_port);
int bootstrap_unregister(Node *node, int ring_id);

/* server.c: servidor TCP, sessões, protocolo e multiplexagem */
int  init_tcp_server(Node *node);
void close_tcp_server(Node *node);
int  handle_new_connection(Node *node);
int  handle_socket_message(Node *node, int fd_type);
void conn_init(Conn *c);
void conn_close(Conn *c);
void conn_drain(Node *node, Conn *c, int fd_type);
int  send_line(int fd, const char *fmt, ...);
int  tcp_connect(const char *ip, int port);
int  read_line_timeout(int fd, char *out, size_t len, int seconds);
int  valid_id(int id);
int  valid_port(int port);
int  valid_ip(const char *ip);
void run_event_loop(Node *node);

/* join.c */
void join_empty(Node *node, int ring_id, int node_id);
void join_existing(Node *node, BootResponse *resp, int node_id);
void join_direct(Node *node, int ring_id, int node_id, int succ_id, const char *succ_ip, int succ_tcp);
int  handle_new(Node *node, Conn *c, int fd_type, int j, const char *ip, int port);
int  handle_id(Node *node, Conn *c, int fd_type, int k);

/* leave.c */
void node_reset(Node *node);
void leave_alone(Node *node);
void leave_multiple(Node *node);
int  handle_con(Node *node, int fd_type, int j, const char *ip, int port);
int  handle_boot(Node *node);

/* search.c */
int  ring_dist(int from, int to);
int  is_responsible(Node *node, int k);
int  pending_add(Node *node, int type, int key, int fd);
void pending_drop_fd(Node *node, int fd);
void pending_clear(Node *node);
void search_start(Node *node, int k);
void search_lookup(Node *node, int k);
void search_resolve(Node *node, int k, int l, const char *l_ip, int l_tcp);
int  handle_qry(Node *node, int j, int k);
int  handle_rsp(Node *node, int j, int k, int l, const char *l_ip, int l_tcp);

#endif

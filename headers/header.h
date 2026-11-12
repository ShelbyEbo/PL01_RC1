#ifndef HEADER_H
# define HEADER_H

# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
# include <string.h>
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

    /* Descritores TCP */
    int server_fd;  /* Socket de escuta TCP (ringport) */
    int pred_fd;    /* Sessão TCP com o predecessor */
    int succ_fd;    /* Sessão TCP com o sucessor */
    int temp_fd;    /* Conexão temporária (ex: adesão, handshake) */
} Node;

typedef struct boot_response
{
    int is_empty;       /* 1 se o anel não tiver nós (EMPTY), 0 se tiver nós */
    int ring_id;
    int boot_node_id;
    char boot_ip[16];
    int boot_tcp;
} BootResponse;

/* Identificadores do tipo de socket para receção de mensagens */
# define FD_TYPE_PRED 1
# define FD_TYPE_SUCC 2
# define FD_TYPE_TEMP 3

/* Protótipos de funções de comandos (process.c) */
void process_command(Node *node, char *command);

/* Protótipos das funções do Bootstrap (bootstrap.c) */
int get_local_ip(const char *dest_ip, int dest_port, char *out_ip, size_t out_len);
int bootstrap_send_recv(Node *node, const char *msg, char *reply, size_t reply_len);
int bootstrap_query(Node *node, int ring_id, BootResponse *res);
int bootstrap_register(Node *node, int ring_id, int node_id, const char *ip, int tcp_port);
int bootstrap_unregister(Node *node, int ring_id);

/* Protótipos do Servidor TCP e Multiplexagem (server.c) */
int init_tcp_server(Node *node);
void close_tcp_server(Node *node);
int handle_new_connection(Node *node);
int handle_socket_message(Node *node, int fd_type);
void run_event_loop(Node *node);

/* Protótipos das funções de adesão aos nós */
void    join_empty();
void    join_existing();

/* Protótipos das funções de saída dos nós */
void leave_alone(Node *node);
void leave_multiple(Node *node);

#endif

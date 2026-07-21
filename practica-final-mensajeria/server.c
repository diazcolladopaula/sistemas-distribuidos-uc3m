/*
 * server.c - Servidor de mensajería concurrente multihilo
 * Sistemas Distribuidos - UC3M - Curso 2025-2026
 *
 * Gestiona: REGISTER, UNREGISTER, CONNECT, DISCONNECT, USERS, SEND, SENDATTACH
 * Parte 2: Integra servidor RPC (LOG_RPC_IP) para registro de operaciones
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>

/* ── RPC includes (Parte 3) ─────────────────────────────────────────── */
#include <rpc/rpc.h>
#include "logRPC.h"   /* generado por rpcgen */

/* ── Constantes ─────────────────────────────────────────────────────── */
#define MAX_USERS       256
#define MAX_MSG_QUEUE   1024
#define MAX_USERNAME    256
#define MAX_MESSAGE     256
#define MAX_FILENAME    256
#define MAX_ID_STR      32
#define BACKLOG         64

/* ── Estructura de mensaje pendiente ─────────────────────────────────── */
typedef struct Message {
    char   sender[MAX_USERNAME];
    char   text[MAX_MESSAGE];
    char   filename[MAX_FILENAME];   /* vacío "" si es SEND sin adjunto */
    unsigned int id;
    struct Message *next;
} Message;

/* ── Estructura de usuario ───────────────────────────────────────────── */
typedef struct {
    char   name[MAX_USERNAME];
    int    connected;       /* 0=desconectado, 1=conectado */
    char   ip[INET_ADDRSTRLEN];
    int    port;
    unsigned int last_msg_id;  /* último id asignado a mensajes enviados POR este usuario */
    Message *pending;       /* cola de mensajes pendientes para este usuario */
    pthread_mutex_t mu;
} User;

/* ── Estado global ───────────────────────────────────────────────────── */
static User      users[MAX_USERS];
static int       user_count = 0;
static pthread_mutex_t users_mu = PTHREAD_MUTEX_INITIALIZER;
static int       server_fd = -1;

/* ── Variable de entorno para RPC ────────────────────────────────────── */
static char rpc_ip[256] = "";

/* ── Helpers de socket ───────────────────────────────────────────────── */

/* Envía una cadena terminada en '\0' */
static int send_str(int fd, const char *s) {
    size_t len = strlen(s) + 1;
    ssize_t sent = 0;
    while ((size_t)sent < len) {
        ssize_t r = write(fd, s + sent, len - (size_t)sent);
        if (r <= 0) return -1;
        sent += r;
    }
    return 0;
}

/* Recibe una cadena terminada en '\0', máx buf_size bytes */
static int recv_str(int fd, char *buf, int buf_size) {
    int i = 0;
    while (i < buf_size - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) return -1;
        buf[i++] = c;
        if (c == '\0') return 0;
    }
    buf[i] = '\0';
    return 0;
}

/* Envía 1 byte de código de resultado */
static int send_code(int fd, unsigned char code) {
    ssize_t r = write(fd, &code, 1);
    return (r == 1) ? 0 : -1;
}

/* ── Búsqueda de usuarios ────────────────────────────────────────────── */

/* Devuelve índice o -1. Llama con users_mu bloqueado. */
static int find_user(const char *name) {
    for (int i = 0; i < user_count; i++)
        if (strcmp(users[i].name, name) == 0) return i;
    return -1;
}

/* ── Notificación al servidor RPC (Parte 3) ──────────────────────────── */
static void rpc_log(const char *username, const char *operation) {
    if (rpc_ip[0] == '\0') return;
    CLIENT *clnt = clnt_create(rpc_ip, LOG_PROG, LOG_VERS, "tcp");
    if (clnt == NULL) return;
    /* LogArgs tiene punteros char* (XDR strings), asignar directamente */
    LogArgs args;
    args.username  = (char *)username;
    args.operation = (char *)operation;
    int result = 0;
    log_op_1(args, &result, clnt);
    clnt_destroy(clnt);
}

/* ── Envío servidor → cliente (protocolo sección 8.6) ───────────────── */
static int deliver_message(const char *dest_ip, int dest_port,
                           const char *sender, unsigned int msg_id,
                           const char *text, const char *filename) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)dest_port);
    if (inet_pton(AF_INET, dest_ip, &addr.sin_addr) <= 0) return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }

    char id_str[MAX_ID_STR];
    snprintf(id_str, sizeof(id_str), "%u", msg_id);

    int ok = 0;
    if (filename[0] != '\0') {
        /* SEND MESSAGE ATTACH */
        ok |= send_str(fd, "SEND MESSAGE ATTACH");
        ok |= send_str(fd, sender);
        ok |= send_str(fd, id_str);
        ok |= send_str(fd, text);
        ok |= send_str(fd, filename);
    } else {
        /* SEND MESSAGE */
        ok |= send_str(fd, "SEND MESSAGE");
        ok |= send_str(fd, sender);
        ok |= send_str(fd, id_str);
        ok |= send_str(fd, text);
    }
    close(fd);
    return ok;
}

/* ── ACK al remitente (sección 8.6 / parte 2) ───────────────────────── */
static void ack_sender(const char *sender_ip, int sender_port,
                       unsigned int msg_id, const char *filename) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)sender_port);
    if (inet_pton(AF_INET, sender_ip, &addr.sin_addr) <= 0) return;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return;
    }

    char id_str[MAX_ID_STR];
    snprintf(id_str, sizeof(id_str), "%u", msg_id);

    if (filename[0] != '\0') {
        send_str(fd, "SEND MESS ATTACH ACK");
        send_str(fd, id_str);
        send_str(fd, filename);
    } else {
        send_str(fd, "SEND MESS ACK");
        send_str(fd, id_str);
    }
    close(fd);
}

/* ── Envía mensajes pendientes al usuario recién conectado ───────────── */
static void flush_pending(int idx) {
    /* Llamar con users[idx].mu bloqueado */
    while (users[idx].pending != NULL) {
        Message *m = users[idx].pending;
        users[idx].pending = m->next;

        /* Guardamos ip/port antes de desbloquear temporalmente */
        char ip[INET_ADDRSTRLEN];
        int  port;
        strncpy(ip, users[idx].ip, sizeof(ip));
        port = users[idx].port;

        int r = deliver_message(ip, port, m->sender, m->id, m->text, m->filename);
        if (r == 0) {
            printf("s> SEND MESSAGE %u FROM %s TO %s\n",
                   m->id, m->sender, users[idx].name);
            /* Buscar remitente para ACK */
            pthread_mutex_lock(&users_mu);
            int si = find_user(m->sender);
            if (si >= 0 && users[si].connected) {
                char sip[INET_ADDRSTRLEN];
                int  sport;
                strncpy(sip, users[si].ip, sizeof(sip));
                sport = users[si].port;
                pthread_mutex_unlock(&users_mu);
                ack_sender(sip, sport, m->id, m->filename);
            } else {
                pthread_mutex_unlock(&users_mu);
            }
            free(m);
        } else {
            /* Reinsertar al frente */
            m->next = users[idx].pending;
            users[idx].pending = m;
            /* Marcar desconectado */
            users[idx].connected = 0;
            memset(users[idx].ip, 0, sizeof(users[idx].ip));
            users[idx].port = 0;
            printf("s> CONNECT %s FAIL\n", users[idx].name);
            break;
        }
    }
}

/* ── Manejadores de operaciones ──────────────────────────────────────── */

static void handle_register(int fd, const char *peer_ip) {
    char username[MAX_USERNAME];
    if (recv_str(fd, username, sizeof(username)) < 0) { send_code(fd, 2); return; }

    pthread_mutex_lock(&users_mu);
    if (find_user(username) >= 0) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 1);
        printf("s> REGISTER %s FAIL\n", username);
        return;
    }
    if (user_count >= MAX_USERS) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 2);
        printf("s> REGISTER %s FAIL\n", username);
        return;
    }
    int idx = user_count++;
    memset(&users[idx], 0, sizeof(users[idx]));
    strncpy(users[idx].name, username, MAX_USERNAME - 1);
    users[idx].connected   = 0;
    users[idx].last_msg_id = 0;
    users[idx].pending     = NULL;
    pthread_mutex_init(&users[idx].mu, NULL);
    pthread_mutex_unlock(&users_mu);

    send_code(fd, 0);
    printf("s> REGISTER %s OK\n", username);
    rpc_log(username, "REGISTER");
    (void)peer_ip;
}

static void handle_unregister(int fd) {
    char username[MAX_USERNAME];
    if (recv_str(fd, username, sizeof(username)) < 0) { send_code(fd, 2); return; }

    pthread_mutex_lock(&users_mu);
    int idx = find_user(username);
    if (idx < 0) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 1);
        printf("s> UNREGISTER %s FAIL\n", username);
        return;
    }
    /* Liberar mensajes pendientes */
    pthread_mutex_lock(&users[idx].mu);
    Message *m = users[idx].pending;
    while (m) { Message *tmp = m->next; free(m); m = tmp; }
    users[idx].pending = NULL;
    pthread_mutex_unlock(&users[idx].mu);

    /* Compactar array */
    for (int i = idx; i < user_count - 1; i++) users[i] = users[idx + 1 + (i - idx)];
    user_count--;
    pthread_mutex_unlock(&users_mu);

    send_code(fd, 0);
    printf("s> UNREGISTER %s OK\n", username);
    rpc_log(username, "UNREGISTER");
}

static void handle_connect(int fd, const char *peer_ip) {
    char username[MAX_USERNAME];
    char port_str[16];
    if (recv_str(fd, username, sizeof(username)) < 0) { send_code(fd, 3); return; }
    if (recv_str(fd, port_str, sizeof(port_str)) < 0)  { send_code(fd, 3); return; }
    int port = atoi(port_str);

    pthread_mutex_lock(&users_mu);
    int idx = find_user(username);
    if (idx < 0) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 1);
        printf("s> CONNECT %s FAIL\n", username);
        return;
    }
    if (users[idx].connected) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 2);
        printf("s> CONNECT %s FAIL\n", username);
        return;
    }
    strncpy(users[idx].ip, peer_ip, INET_ADDRSTRLEN - 1);
    users[idx].port      = port;
    users[idx].connected = 1;
    pthread_mutex_unlock(&users_mu);

    send_code(fd, 0);
    printf("s> CONNECT %s OK\n", username);
    rpc_log(username, "CONNECT");

    /* Enviar mensajes pendientes */
    pthread_mutex_lock(&users[idx].mu);
    flush_pending(idx);
    pthread_mutex_unlock(&users[idx].mu);
}

static void handle_disconnect(int fd) {
    char username[MAX_USERNAME];
    if (recv_str(fd, username, sizeof(username)) < 0) { send_code(fd, 3); return; }

    pthread_mutex_lock(&users_mu);
    int idx = find_user(username);
    if (idx < 0) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 1);
        printf("s> DISCONNECT %s FAIL\n", username);
        return;
    }
    if (!users[idx].connected) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 2);
        printf("s> DISCONNECT %s FAIL\n", username);
        return;
    }
    users[idx].connected = 0;
    memset(users[idx].ip, 0, sizeof(users[idx].ip));
    users[idx].port = 0;
    pthread_mutex_unlock(&users_mu);

    send_code(fd, 0);
    printf("s> DISCONNECT %s OK\n", username);
    rpc_log(username, "DISCONNECT");
}

static void handle_users(int fd) {
    char username[MAX_USERNAME];
    if (recv_str(fd, username, sizeof(username)) < 0) { send_code(fd, 2); return; }

    pthread_mutex_lock(&users_mu);
    int idx = find_user(username);
    if (idx < 0 || !users[idx].connected) {
        int code = (idx < 0) ? 2 : 1;
        pthread_mutex_unlock(&users_mu);
        send_code(fd, (unsigned char)code);
        printf("s> CONNECTEDUSERS FAIL\n");
        return;
    }

    /* Contar conectados */
    int cnt = 0;
    for (int i = 0; i < user_count; i++)
        if (users[i].connected) cnt++;

    send_code(fd, 0);
    char cnt_str[16];
    snprintf(cnt_str, sizeof(cnt_str), "%d", cnt);
    send_str(fd, cnt_str);

    /* Enviar cada usuario conectado: en Parte 1 solo nombre; en Parte 2 nombre::IP::puerto */
    for (int i = 0; i < user_count; i++) {
        if (users[i].connected) {
            char entry[MAX_USERNAME + INET_ADDRSTRLEN + 16];
            snprintf(entry, sizeof(entry), "%s :: %s :: %d",
                     users[i].name, users[i].ip, users[i].port);
            send_str(fd, entry);
        }
    }
    pthread_mutex_unlock(&users_mu);
    printf("s> CONNECTEDUSERS OK\n");
    rpc_log(username, "USERS");
}

/* Común a SEND y SENDATTACH */
static void handle_send_common(int fd, int with_attach) {
    char sender[MAX_USERNAME], dest[MAX_USERNAME];
    char text[MAX_MESSAGE], filename[MAX_FILENAME];
    filename[0] = '\0';

    if (recv_str(fd, sender,   sizeof(sender))   < 0) { send_code(fd, 2); return; }
    if (recv_str(fd, dest,     sizeof(dest))     < 0) { send_code(fd, 2); return; }
    if (recv_str(fd, text,     sizeof(text))     < 0) { send_code(fd, 2); return; }
    if (with_attach)
        if (recv_str(fd, filename, sizeof(filename)) < 0) { send_code(fd, 2); return; }

    pthread_mutex_lock(&users_mu);
    int didx = find_user(dest);
    int sidx = find_user(sender);
    if (didx < 0) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 1);
        return;
    }
    if (sidx < 0) {
        pthread_mutex_unlock(&users_mu);
        send_code(fd, 2);
        return;
    }

    /* Asignar id al mensaje usando el contador del REMITENTE */
    pthread_mutex_lock(&users[sidx].mu);
    users[sidx].last_msg_id++;
    if (users[sidx].last_msg_id == 0) users[sidx].last_msg_id = 1; /* skip 0 */
    unsigned int msg_id = users[sidx].last_msg_id;
    pthread_mutex_unlock(&users[sidx].mu);

    /* Encolar en pendientes del destinatario */
    Message *m = (Message *)malloc(sizeof(Message));
    if (!m) { pthread_mutex_unlock(&users_mu); send_code(fd, 2); return; }
    strncpy(m->sender,   sender,   MAX_USERNAME - 1);
    strncpy(m->text,     text,     MAX_MESSAGE  - 1);
    strncpy(m->filename, filename, MAX_FILENAME - 1);
    m->sender[MAX_USERNAME-1] = m->text[MAX_MESSAGE-1] = m->filename[MAX_FILENAME-1] = '\0';
    m->id   = msg_id;
    m->next = NULL;

    pthread_mutex_lock(&users[didx].mu);
    /* Append al final */
    if (users[didx].pending == NULL) {
        users[didx].pending = m;
    } else {
        Message *tail = users[didx].pending;
        while (tail->next) tail = tail->next;
        tail->next = m;
    }
    pthread_mutex_unlock(&users[didx].mu);

    /* Responder al remitente con código 0 + id */
    send_code(fd, 0);
    char id_str[MAX_ID_STR];
    snprintf(id_str, sizeof(id_str), "%u", msg_id);
    send_str(fd, id_str);

    /* Si destino conectado, entregar ahora */
    int dest_connected = users[didx].connected;
    char dest_ip[INET_ADDRSTRLEN];
    int  dest_port = users[didx].port;
    strncpy(dest_ip, users[didx].ip, sizeof(dest_ip));

    char src_ip[INET_ADDRSTRLEN];
    int  src_port = users[sidx].port;
    int  src_connected = users[sidx].connected;
    strncpy(src_ip, users[sidx].ip, sizeof(src_ip));

    pthread_mutex_unlock(&users_mu);

    if (dest_connected) {
        /* Quitar de pendientes y entregar */
        pthread_mutex_lock(&users[didx].mu);
        /* Buscar el mensaje */
        Message **pp = &users[didx].pending;
        Message *found = NULL;
        while (*pp) {
            if ((*pp)->id == msg_id && strcmp((*pp)->sender, sender) == 0) {
                found = *pp;
                *pp = found->next;
                break;
            }
            pp = &(*pp)->next;
        }
        pthread_mutex_unlock(&users[didx].mu);

        if (found) {
            int r = deliver_message(dest_ip, dest_port, sender, msg_id, text, filename);
            if (r == 0) {
                printf("s> SEND MESSAGE %u FROM %s TO %s\n", msg_id, sender, dest);
                if (src_connected) {
                    ack_sender(src_ip, src_port, msg_id, filename);
                }
                free(found);
            } else {
                /* Reencolar */
                pthread_mutex_lock(&users[didx].mu);
                found->next = users[didx].pending;
                users[didx].pending = found;
                users[didx].connected = 0;
                pthread_mutex_unlock(&users[didx].mu);
            }
        }
    } else {
        printf("s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, sender, dest);
    }

    if (with_attach) {
        char op[MAX_MESSAGE + MAX_FILENAME];
        snprintf(op, sizeof(op), "SENDATTACH %s", filename);
        rpc_log(sender, op);
    } else {
        rpc_log(sender, "SEND");
    }
}

/* ── Hilo por conexión entrante ──────────────────────────────────────── */
typedef struct { int fd; char ip[INET_ADDRSTRLEN]; } ConnArg;

static void *handle_connection(void *arg) {
    ConnArg *ca = (ConnArg *)arg;
    int fd = ca->fd;
    char peer_ip[INET_ADDRSTRLEN];
    strncpy(peer_ip, ca->ip, sizeof(peer_ip));
    free(ca);

    char op[64];
    if (recv_str(fd, op, sizeof(op)) < 0) { close(fd); return NULL; }

    if      (strcmp(op, "REGISTER")   == 0) handle_register(fd, peer_ip);
    else if (strcmp(op, "UNREGISTER") == 0) handle_unregister(fd);
    else if (strcmp(op, "CONNECT")    == 0) handle_connect(fd, peer_ip);
    else if (strcmp(op, "DISCONNECT") == 0) handle_disconnect(fd);
    else if (strcmp(op, "USERS")      == 0) handle_users(fd);
    else if (strcmp(op, "SEND")       == 0) handle_send_common(fd, 0);
    else if (strcmp(op, "SENDATTACH") == 0) handle_send_common(fd, 1);

    close(fd);
    return NULL;
}

/* ── SIGINT handler ──────────────────────────────────────────────────── */
static void sigint_handler(int sig) {
    (void)sig;
    if (server_fd >= 0) close(server_fd);
    exit(0);
}

/* ── main ────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    /* Salida en modo linea-por-linea: evita perder logs "s> ..." si el
       proceso se termina con kill/SIGTERM en vez de Ctrl+C. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    int port = -1;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            port = atoi(argv[i + 1]);
        }
    }
    if (port < 1) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        return 1;
    }

    /* Leer variable de entorno para RPC */
    const char *env = getenv("LOG_RPC_IP");
    if (env) strncpy(rpc_ip, env, sizeof(rpc_ip) - 1);

    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Crear socket servidor */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen"); return 1;
    }

    /* Obtener IP local */
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct hostent *he = gethostbyname(hostname);
    char local_ip[INET_ADDRSTRLEN] = "127.0.0.1";
    if (he && he->h_addr_list[0])
        inet_ntop(AF_INET, he->h_addr_list[0], local_ip, sizeof(local_ip));

    printf("s> init server %s:%d\n", local_ip, port);
    printf("s> ");
    fflush(stdout);

    /* Bucle de aceptación */
    while (1) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(server_fd, (struct sockaddr *)&cli, &cli_len);
        if (cfd < 0) {
            if (errno == EINTR) break;
            continue;
        }

        ConnArg *ca = (ConnArg *)malloc(sizeof(ConnArg));
        if (!ca) { close(cfd); continue; }
        ca->fd = cfd;
        inet_ntop(AF_INET, &cli.sin_addr, ca->ip, sizeof(ca->ip));

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&tid, &attr, handle_connection, ca) != 0) {
            free(ca); close(cfd);
        }
        pthread_attr_destroy(&attr);
    }

    close(server_fd);
    return 0;
}

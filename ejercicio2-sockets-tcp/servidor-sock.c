/*
 * Servidor concurrente de tuplas clave-valor usando sockets TCP.
 * Uso ./servidor <PUERTO>
 *
 * Por cada cliente que se conecta se crea un hilo que atiende su petición
 * y cierra la conexión al terminar. La lógica de almacenamiento se delega
 * en claves.c (libclaves.so), que ya gestiona su propio mutex.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "claves.h"
#include "protocolo.h"


//  Utilidades de E/S garantizada
static int send_all(int sock, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t s = send(sock, p, len, 0);
        if (s <= 0) return -1;
        p += s; len -= s;
    }
    return 0;
}

static int recv_all(int sock, void *buf, size_t len) {
    char *p = (char *)buf;
    while (len > 0) {
        ssize_t r = recv(sock, p, len, 0);
        if (r <= 0) return -1;
        p += r; len -= r;
    }
    return 0;
}

static int send_int32(int sock, int32_t val) {
    uint32_t net = htonl((uint32_t)val);
    return send_all(sock, &net, 4);
}

static int recv_int32(int sock, int32_t *val) {
    uint32_t net;
    if (recv_all(sock, &net, 4) < 0) return -1;
    *val = (int32_t)ntohl(net);
    return 0;
}

static int send_float(int sock, float f) {
    uint32_t bits;
    memcpy(&bits, &f, 4);
    bits = htonl(bits);
    return send_all(sock, &bits, 4);
}

static int recv_float(int sock, float *f) {
    uint32_t bits;
    if (recv_all(sock, &bits, 4) < 0) return -1;
    bits = ntohl(bits);
    memcpy(f, &bits, 4);
    return 0;
}

static int send_string(int sock, const char *str, size_t field_size) {
    char buf[VALUE1_SIZE];
    memset(buf, 0, field_size);
    strncpy(buf, str, field_size - 1);
    return send_all(sock, buf, field_size);
}

static int recv_string(int sock, char *str, size_t field_size) {
    if (recv_all(sock, str, field_size) < 0) return -1;
    str[field_size - 1] = '\0';
    return 0;
}


// funciones de atención a cada operación 
static void handle_destroy(int sock) {
    int res = destroy();
    send_int32(sock, res);
}

static void handle_set(int sock) {
    char key[KEY_SIZE], value1[VALUE1_SIZE];
    int32_t n2;
    float v2[MAX_V2];
    int32_t vx, vy, vz;

    if (recv_string(sock, key,    KEY_SIZE)    < 0) return;
    if (recv_string(sock, value1, VALUE1_SIZE) < 0) return;
    if (recv_int32(sock, &n2) < 0) return;
    if (n2 < 1 || n2 > MAX_V2) { send_int32(sock, -1); return; }
    for (int i = 0; i < n2; i++)
        if (recv_float(sock, &v2[i]) < 0) return;
    if (recv_int32(sock, &vx) < 0) return;
    if (recv_int32(sock, &vy) < 0) return;
    if (recv_int32(sock, &vz) < 0) return;

    struct Paquete p = { (int)vx, (int)vy, (int)vz };
    int res = set_value(key, value1, (int)n2, v2, p);
    send_int32(sock, res);
}

static void handle_get(int sock) {
    char key[KEY_SIZE];
    if (recv_string(sock, key, KEY_SIZE) < 0) return;

    char value1[VALUE1_SIZE];
    int  n2;
    float v2[MAX_V2];
    struct Paquete p;

    int res = get_value(key, value1, &n2, v2, &p);
    if (send_int32(sock, res) < 0) return;

    if (res == 0) {
        if (send_string(sock, value1, VALUE1_SIZE) < 0) return;
        if (send_int32(sock, n2) < 0) return;
        for (int i = 0; i < n2; i++)
            if (send_float(sock, v2[i]) < 0) return;
        if (send_int32(sock, p.x) < 0) return;
        if (send_int32(sock, p.y) < 0) return;
        if (send_int32(sock, p.z) < 0) return;
    }
}

static void handle_modify(int sock) {
    char key[KEY_SIZE], value1[VALUE1_SIZE];
    int32_t n2;
    float v2[MAX_V2];
    int32_t vx, vy, vz;

    if (recv_string(sock, key,    KEY_SIZE)    < 0) return;
    if (recv_string(sock, value1, VALUE1_SIZE) < 0) return;
    if (recv_int32(sock, &n2) < 0) return;
    if (n2 < 1 || n2 > MAX_V2) { send_int32(sock, -1); return; }
    for (int i = 0; i < n2; i++)
        if (recv_float(sock, &v2[i]) < 0) return;
    if (recv_int32(sock, &vx) < 0) return;
    if (recv_int32(sock, &vy) < 0) return;
    if (recv_int32(sock, &vz) < 0) return;

    struct Paquete p = { (int)vx, (int)vy, (int)vz };
    int res = modify_value(key, value1, (int)n2, v2, p);
    send_int32(sock, res);
}

static void handle_delete(int sock) {
    char key[KEY_SIZE];
    if (recv_string(sock, key, KEY_SIZE) < 0) return;
    int res = delete_key(key);
    send_int32(sock, res);
}

static void handle_exist(int sock) {
    char key[KEY_SIZE];
    if (recv_string(sock, key, KEY_SIZE) < 0) return;
    int res = exist(key);
    send_int32(sock, res);
}


//Hilo de atención a un cliente


static void *worker(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    pthread_detach(pthread_self()); // el hilo se limpia solo al terminar

    int32_t op;
    if (recv_int32(sock, &op) < 0) {
        close(sock);
        return NULL;
    }

    switch (op) {
        case OP_DESTROY: handle_destroy(sock); break;
        case OP_SET:     handle_set(sock);     break;
        case OP_GET:     handle_get(sock);     break;
        case OP_MODIFY:  handle_modify(sock);  break;
        case OP_DELETE:  handle_delete(sock);  break;
        case OP_EXIST:   handle_exist(sock);   break;
        default:
            fprintf(stderr, "Operación desconocida: %d\n", (int)op);
            send_int32(sock, -1);
            break;
    }

    close(sock);
    return NULL;
}


//      Programa principal


int main(int argc, char *argv[]) {
    /* Salida en modo linea-por-linea: evita perder logs si el proceso
       se termina con kill/SIGTERM en vez de Ctrl+C. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0) {
        fprintf(stderr, "Puerto no válido: %s\n", argv[1]);
        return 1;
    }

    // creamos el socket de escucha
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return 1;
    }

    // permitimos reusar el puerto inmediatamente tras reiniciar
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_sock);
        return 1;
    }

    if (listen(listen_sock, 10) < 0) {
        perror("listen");
        close(listen_sock);
        return 1;
    }

    printf("Servidor TCP de tuplas escuchando en el puerto %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_sock = accept(listen_sock,
                                 (struct sockaddr *)&client_addr,
                                 &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        printf("Conexión aceptada de %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        //  pasamos el fd al hilo mediante un puntero en heap (evita race)
        int *p_sock = malloc(sizeof(int));
        if (p_sock == NULL) {
            fprintf(stderr, "ERROR: malloc fallido\n");
            close(client_sock);
            continue;
        }
        *p_sock = client_sock;

        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, p_sock) != 0) {
            perror("pthread_create");
            free(p_sock);
            close(client_sock);
        }
        // El hilo se encarga de hacer detach y cerrar el socket
    }

    close(listen_sock);
    return 0;
}

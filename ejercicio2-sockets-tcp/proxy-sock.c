/*
 * implementacion de la biblioteca cliente (libproxyclaves.so) que se comunica
 * con el servidor mediante sockets TCP.
 *
 * La IP del servidor se lee de la variable de entorno IP_TUPLAS.
 * El puerto se lee de la variable de entorno PORT_TUPLAS.
 */

#include "claves.h"
#include "protocolo.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* envia exactamente 'len' bytes. Devuelve 0 en exito, -1 en error. */
static int send_all(int sock, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  while (len > 0) {
    ssize_t sent = send(sock, p, len, 0);
    if (sent <= 0)
      return -1;
    p += sent;
    len -= sent;
  }
  return 0;
}

/* recibe exactamente 'len' bytes. Devuelve 0 en exito, -1 en error. */
static int recv_all(int sock, void *buf, size_t len) {
  char *p = (char *)buf;
  while (len > 0) {
    ssize_t r = recv(sock, p, len, 0);
    if (r <= 0)
      return -1;
    p += r;
    len -= r;
  }
  return 0;
}

static int send_int32(int sock, int32_t val) {
  uint32_t net = htonl((uint32_t)val);
  return send_all(sock, &net, 4);
}

static int recv_int32(int sock, int32_t *val) {
  uint32_t net;
  if (recv_all(sock, &net, 4) < 0)
    return -1;
  *val = (int32_t)ntohl(net);
  return 0;
}

/* envia un float como 4 bytes IEEE 754 big-endian */
static int send_float(int sock, float f) {
  uint32_t bits;
  memcpy(&bits, &f, 4);
  bits = htonl(bits);
  return send_all(sock, &bits, 4);
}

/* recibe un float desde 4 bytes IEEE 754 big-endian */
static int recv_float(int sock, float *f) {
  uint32_t bits;
  if (recv_all(sock, &bits, 4) < 0)
    return -1;
  bits = ntohl(bits);
  memcpy(f, &bits, 4);
  return 0;
}

/* envia una cadena como campo fijo de KEY_SIZE/VALUE1_SIZE bytes */
static int send_string(int sock, const char *str, size_t field_size) {
  char buf[VALUE1_SIZE]; /* field_size <= VALUE1_SIZE siempre */
  memset(buf, 0, field_size);
  strncpy(buf, str, field_size - 1);
  return send_all(sock, buf, field_size);
}

/* recibe una cadena desde campo fijo */
static int recv_string(int sock, char *str, size_t field_size) {
  char buf[VALUE1_SIZE];
  if (recv_all(sock, buf, field_size) < 0)
    return -1;
  buf[field_size - 1] = '\0';
  memcpy(str, buf, field_size);
  return 0;
}

static int conectar(void) {
  char *ip_env = getenv("IP_TUPLAS");
  char *port_env = getenv("PORT_TUPLAS");

  if (ip_env == NULL || port_env == NULL) {
    fprintf(stderr,
            "ERROR: Define las variables de entorno IP_TUPLAS y PORT_TUPLAS\n");
    return -1;
  }

  int port = atoi(port_env);
  if (port <= 0) {
    fprintf(stderr, "ERROR: PORT_TUPLAS no es un número de puerto válido\n");
    return -1;
  }

  /* resolución del nombre (acepta tanto decimal-punto como dominio-punto) */
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  if (getaddrinfo(ip_env, port_str, &hints, &res) != 0) {
    fprintf(stderr, "ERROR: No se puede resolver la dirección '%s'\n", ip_env);
    return -1;
  }

  int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock < 0) {
    freeaddrinfo(res);
    return -1;
  }

  if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
    close(sock);
    freeaddrinfo(res);
    fprintf(stderr, "ERROR: No se puede conectar con el servidor %s:%d\n",
            ip_env, port);
    return -1;
  }

  freeaddrinfo(res);
  return sock;
}

int destroy(void) {
  int sock = conectar();
  if (sock < 0)
    return -1;

  int ret = -1;
  if (send_int32(sock, OP_DESTROY) < 0)
    goto fin;

  int32_t resultado;
  if (recv_int32(sock, &resultado) < 0)
    goto fin;
  ret = (int)resultado;

fin:
  close(sock);
  return ret;
}

int set_value(char *key, char *value1, int N_value2, float *V_value2,
              struct Paquete value3) {
  if (N_value2 < 1 || N_value2 > MAX_V2)
    return -1;
  if (key == NULL || value1 == NULL)
    return -1;

  int sock = conectar();
  if (sock < 0)
    return -1;

  int ret = -1;

  if (send_int32(sock, OP_SET) < 0)
    goto fin;
  if (send_string(sock, key, KEY_SIZE) < 0)
    goto fin;
  if (send_string(sock, value1, VALUE1_SIZE) < 0)
    goto fin;
  if (send_int32(sock, N_value2) < 0)
    goto fin;
  for (int i = 0; i < N_value2; i++)
    if (send_float(sock, V_value2[i]) < 0)
      goto fin;
  if (send_int32(sock, value3.x) < 0)
    goto fin;
  if (send_int32(sock, value3.y) < 0)
    goto fin;
  if (send_int32(sock, value3.z) < 0)
    goto fin;

  int32_t resultado;
  if (recv_int32(sock, &resultado) < 0)
    goto fin;
  ret = (int)resultado;

fin:
  close(sock);
  return ret;
}

int get_value(char *key, char *value1, int *N_value2, float *V_value2,
              struct Paquete *value3) {
  if (key == NULL)
    return -1;

  int sock = conectar();
  if (sock < 0)
    return -1;

  int ret = -1;

  if (send_int32(sock, OP_GET) < 0)
    goto fin;
  if (send_string(sock, key, KEY_SIZE) < 0)
    goto fin;

  int32_t resultado;
  if (recv_int32(sock, &resultado) < 0)
    goto fin;

  if (resultado == 0) {
    /* recibimos los datos asociados a la clave */
    if (recv_string(sock, value1, VALUE1_SIZE) < 0)
      goto fin;

    int32_t n2;
    if (recv_int32(sock, &n2) < 0)
      goto fin;
    *N_value2 = (int)n2;

    for (int i = 0; i < n2; i++)
      if (recv_float(sock, &V_value2[i]) < 0)
        goto fin;

    int32_t vx, vy, vz;
    if (recv_int32(sock, &vx) < 0)
      goto fin;
    if (recv_int32(sock, &vy) < 0)
      goto fin;
    if (recv_int32(sock, &vz) < 0)
      goto fin;
    value3->x = (int)vx;
    value3->y = (int)vy;
    value3->z = (int)vz;
  }
  ret = (int)resultado;

fin:
  close(sock);
  return ret;
}

int modify_value(char *key, char *value1, int N_value2, float *V_value2,
                 struct Paquete value3) {
  if (N_value2 < 1 || N_value2 > MAX_V2)
    return -1;
  if (key == NULL || value1 == NULL)
    return -1;

  int sock = conectar();
  if (sock < 0)
    return -1;

  int ret = -1;

  if (send_int32(sock, OP_MODIFY) < 0)
    goto fin;
  if (send_string(sock, key, KEY_SIZE) < 0)
    goto fin;
  if (send_string(sock, value1, VALUE1_SIZE) < 0)
    goto fin;
  if (send_int32(sock, N_value2) < 0)
    goto fin;
  for (int i = 0; i < N_value2; i++)
    if (send_float(sock, V_value2[i]) < 0)
      goto fin;
  if (send_int32(sock, value3.x) < 0)
    goto fin;
  if (send_int32(sock, value3.y) < 0)
    goto fin;
  if (send_int32(sock, value3.z) < 0)
    goto fin;

  int32_t resultado;
  if (recv_int32(sock, &resultado) < 0)
    goto fin;
  ret = (int)resultado;

fin:
  close(sock);
  return ret;
}

int delete_key(char *key) {
  if (key == NULL)
    return -1;

  int sock = conectar();
  if (sock < 0)
    return -1;

  int ret = -1;

  if (send_int32(sock, OP_DELETE) < 0)
    goto fin;
  if (send_string(sock, key, KEY_SIZE) < 0)
    goto fin;

  int32_t resultado;
  if (recv_int32(sock, &resultado) < 0)
    goto fin;
  ret = (int)resultado;

fin:
  close(sock);
  return ret;
}

int exist(char *key) {
  if (key == NULL)
    return -1;

  int sock = conectar();
  if (sock < 0)
    return -1;

  int ret = -1;

  if (send_int32(sock, OP_EXIST) < 0)
    goto fin;
  if (send_string(sock, key, KEY_SIZE) < 0)
    goto fin;

  int32_t resultado;
  if (recv_int32(sock, &resultado) < 0)
    goto fin;
  ret = (int)resultado;

fin:
  close(sock);
  return ret;
}

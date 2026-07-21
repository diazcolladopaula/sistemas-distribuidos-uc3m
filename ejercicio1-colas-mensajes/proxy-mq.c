#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include <pthread.h>
#include "claves.h"
#include "mensajes.h"

// función privada auxiliar para gestionar la comunicación con el servidor
int enviar_y_recibir(struct Mensaje *msg) {
    char nombre_cola_cliente[64];
    
    // generamos un nombre unico para la cola de este cliente (usando su PID e ID de hilo)
    sprintf(nombre_cola_cliente, "/cola_cliente_%d_%ld", getpid(), (long)pthread_self());
    strcpy(msg->cola_respuesta, nombre_cola_cliente);

    // configuramos y creamos nuestra cola buzon para recibir la respuesta
    struct mq_attr attr;
    attr.mq_maxmsg = 1; // solo esperamos 1 mensaje de respuesta
    attr.mq_msgsize = sizeof(struct Mensaje);
    attr.mq_flags = 0;

    mq_unlink(nombre_cola_cliente); // por si se quedo abierta en un fallo anterior
    mqd_t q_cliente = mq_open(nombre_cola_cliente, O_CREAT | O_RDWR, 0666, &attr);
    if (q_cliente == (mqd_t)-1) return -2; // error de comunicaciones

    // abrimos la cola del servidor para enviarle la peticion
    mqd_t q_servidor = mq_open(COLA_SERVIDOR, O_WRONLY);
    if (q_servidor == (mqd_t)-1) {
        mq_close(q_cliente);
        mq_unlink(nombre_cola_cliente);
        return -2; // error el servidor no esta encendido
    }

    // enviamos el mensaje al servidor
    if (mq_send(q_servidor, (char *)msg, sizeof(struct Mensaje), 0) == -1) {
        mq_close(q_servidor); mq_close(q_cliente); mq_unlink(nombre_cola_cliente);
        return -2; // error al enviar
    }

    // nos quedamos bloqueados esperando que el servidor nos responda en nuestra cola
    if (mq_receive(q_cliente, (char *)msg, sizeof(struct Mensaje), NULL) == -1) {
        mq_close(q_servidor); mq_close(q_cliente); mq_unlink(nombre_cola_cliente);
        return -2; // error al recibir
    }

    // limpiamos y cerramos las colas
    mq_close(q_servidor);
    mq_close(q_cliente);
    mq_unlink(nombre_cola_cliente);

    return msg->resultado; // devolvemos el 0 o -1 que calculo el servidor
}

// implementacion de la API (Proxy)
int destroy(void) {
    struct Mensaje msg;
    msg.operacion = OP_INIT;
    return enviar_y_recibir(&msg);
}

int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    // validacion local si hay un error, no molestamos al servidor
    if (N_value2 < 1 || N_value2 > 32) return -1;
    if (key == NULL || value1 == NULL) return -1;

    struct Mensaje msg;
    msg.operacion = OP_SET;
    strncpy(msg.key, key, 255); msg.key[255] = '\0';
    strncpy(msg.value1, value1, 255); msg.value1[255] = '\0';
    msg.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) msg.V_value2[i] = V_value2[i];
    msg.value3 = value3;

    return enviar_y_recibir(&msg);
}

int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    if (key == NULL) return -1;

    struct Mensaje msg;
    msg.operacion = OP_GET;
    strncpy(msg.key, key, 255); msg.key[255] = '\0';

    int res = enviar_y_recibir(&msg);

    // si el servidor encontro la clave (res == 0), extraemos los datos del mensaje 
    // y los copiamos en los punteros que nos paso el cliente
    if (res == 0) {
        strcpy(value1, msg.value1);
        *N_value2 = msg.N_value2;
        for (int i = 0; i < msg.N_value2; i++) V_value2[i] = msg.V_value2[i];
        *value3 = msg.value3;
    }
    return res;
}

int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (N_value2 < 1 || N_value2 > 32) return -1;
    if (key == NULL || value1 == NULL) return -1;
    struct Mensaje msg;
    msg.operacion = OP_MODIFY;
    strncpy(msg.key, key, 255); msg.key[255] = '\0';
    strncpy(msg.value1, value1, 255); msg.value1[255] = '\0';
    msg.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) msg.V_value2[i] = V_value2[i];
    msg.value3 = value3;
    return enviar_y_recibir(&msg);
}

int delete_key(char *key) {
    if (key == NULL) return -1;

    struct Mensaje msg;
    msg.operacion = OP_DELETE;
    strncpy(msg.key, key, 255); msg.key[255] = '\0';
    return enviar_y_recibir(&msg);
}

int exist(char *key) {
    if (key == NULL) return -1;

    struct Mensaje msg;
    msg.operacion = OP_EXIST;
    strncpy(msg.key, key, 255); msg.key[255] = '\0';
    return enviar_y_recibir(&msg);
}
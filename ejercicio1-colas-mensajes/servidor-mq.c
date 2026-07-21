#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mensajes.h"
#include "claves.h"

// funcion que ejecutara cada hilo para procesar la peticion
void *procesar_peticion(void *arg) {
    // recuperamos el mensaje que nos ha pasado el hilo principal
    struct Mensaje *msg = (struct Mensaje *)arg;
    
    printf("[Servidor] Procesando operación %d para la cola %s\n", msg->operacion, msg->cola_respuesta);

    // ejecutamos la funcion real dependiendo de la operacion solicitada
    switch (msg->operacion) {
        case OP_INIT:
            msg->resultado = destroy();
            break;
            
        case OP_SET:
            msg->resultado = set_value(msg->key, msg->value1, msg->N_value2, msg->V_value2, msg->value3);
            break;
            
        case OP_GET:
            msg->resultado = get_value(msg->key, msg->value1, &msg->N_value2, msg->V_value2, &msg->value3);
            break;
            
        case OP_MODIFY:
            msg->resultado = modify_value(msg->key, msg->value1, msg->N_value2, msg->V_value2, msg->value3);
            break;
            
        case OP_DELETE:
            msg->resultado = delete_key(msg->key);
            break;
            
        case OP_EXIST:
            msg->resultado = exist(msg->key);
            break;
            
        default:
            printf("[Servidor] Error: Operacion desconocida.\n");
            msg->resultado = -1;
            break;
    }

    // abrimos la cola del cliente para enviarle la respuesta
    mqd_t q_cliente = mq_open(msg->cola_respuesta, O_WRONLY);
    if (q_cliente == (mqd_t)-1) {
        perror("[Servidor] Error al abrir la cola del cliente");
    } else {
    // enviamos el mensaje de vuelta con el resultado y los datos
        if (mq_send(q_cliente, (const char *)msg, sizeof(struct Mensaje), 0) == -1) {
            perror("[Servidor] Error al enviar respuesta al cliente");
        }
        mq_close(q_cliente); // cerramos nuestra conexion con la cola del cliente
    }

    // liberamos la memoria del mensaje y terminamos el hilo
    free(msg);
    pthread_exit(NULL);
}

int main() {
    /* Salida en modo linea-por-linea: sin esto, los logs "s> ..." quedan
       atrapados en el buffer de stdio si el proceso se termina con kill/SIGTERM
       en vez de Ctrl+C, y se pierden por completo. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    mqd_t q_servidor;
    struct mq_attr attr;

    // configuracion de la cola del servidor
    attr.mq_maxmsg = 10; // maximo 10 mensajes en espera
    attr.mq_msgsize = sizeof(struct Mensaje); // tamanio de nuestra caja
    attr.mq_flags = 0;

    // borramos la cola por si se quedo colgada en una ejecucion anterior
    mq_unlink(COLA_SERVIDOR);

    // creamos y abrimos la cola del servidor
    q_servidor = mq_open(COLA_SERVIDOR, O_CREAT | O_RDONLY, 0666, &attr);
    if (q_servidor == (mqd_t)-1) {
        perror("Error al crear la cola del servidor");
        return -1;
    }

    printf("=== Servidor de Claves Iniciado ===\n");
    printf("Escuchando en la cola: %s\n", COLA_SERVIDOR);

    // bucle infinito del servidor
    while (1) {
        // reservamos memoria para recibir un mensaje nuevo
        struct Mensaje *msg_recibido = (struct Mensaje *)malloc(sizeof(struct Mensaje));
        if (msg_recibido == NULL) {
            perror("Error de memoria");
            continue;
        }

        // esperamos bloqueados hasta que llegue un mensaje
        if (mq_receive(q_servidor, (char *)msg_recibido, sizeof(struct Mensaje), NULL) == -1) {
            perror("Error al recibir mensaje");
            free(msg_recibido);
            continue;
        }

        // creamos un hilo para atender esta peticion de forma concurrente
        pthread_t hilo;
        pthread_attr_t attr_hilo;
        pthread_attr_init(&attr_hilo);
        // hacemos el hilo detached para que libere sus recursos al terminar
        pthread_attr_setdetachstate(&attr_hilo, PTHREAD_CREATE_DETACHED); 

        if (pthread_create(&hilo, &attr_hilo, procesar_peticion, (void *)msg_recibido) != 0) {
            perror("Error al crear el hilo");
            free(msg_recibido);
        }
        
        pthread_attr_destroy(&attr_hilo);
    }

    // este codigo nunca se ejecutara por el while pero es buena practica ponerlo
    mq_close(q_servidor);
    mq_unlink(COLA_SERVIDOR);
    return 0;
}
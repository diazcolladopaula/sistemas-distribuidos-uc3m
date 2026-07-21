#ifndef MENSAJES_H
#define MENSAJES_H
#include "claves.h"
// definimos codigos para saber que operacion nos estan pidiendo
#define OP_INIT   0
#define OP_SET    1
#define OP_GET    2
#define OP_MODIFY 3
#define OP_DELETE 4
#define OP_EXIST  5

// estructura del mensaje que viajara por las colas POSIX
struct Mensaje {
    int operacion;          // codigo de la operación
    char key[256];          // la clave
    char value1[256];       // el primer valor
    int N_value2;           // tamanio del vector
    float V_value2[32];     // el vector de floats
    struct Paquete value3;  // la estructura con x, y, z
    
    int resultado;          // aqui el servidor metera el 0, 1 o -1 de vuelta
    char cola_respuesta[64];// el nombre de la cola donde el cliente espera la respuesta
};

// nombres base para las colas
#define COLA_SERVIDOR "/cola_servidor_claves"

#endif
#include "claves.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// definicion de la estructura del nodo para la lista enlazada
struct Nodo {
  char key[256];
  char value1[256];
  int N_value2;
  float V_value2[32];
  struct Paquete value3;
  struct Nodo *siguiente;
};

// inicio de la lista y mutex
struct Nodo *cabeza = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int destroy(void) {
  pthread_mutex_lock(&mutex); // bloqueamos acceso

  struct Nodo *actual = cabeza;
  struct Nodo *siguiente;

  // recorremos la lista liberando la memoria de cada nodo
  while (actual != NULL) {
    siguiente = actual->siguiente;
    free(actual);
    actual = siguiente;
  }

  cabeza = NULL; // reiniciar lista

  pthread_mutex_unlock(&mutex); // desbloqueamos el acceso
  return 0;
}

int set_value(char *key, char *value1, int N_value2, float *V_value2,
              struct Paquete value3) {
  // comprobamos restricciones antes de bloquear
  if (N_value2 < 1 || N_value2 > 32)
    return -1;
  if (key == NULL || value1 == NULL)
    return -1;

  pthread_mutex_lock(&mutex);

  // comprobamos si la clave ya existe
  struct Nodo *actual = cabeza;
  while (actual != NULL) {
    if (strcmp(actual->key, key) == 0) {
      pthread_mutex_unlock(&mutex);
      return -1; // error la clave ya existe
    }
    actual = actual->siguiente;
  }

  // reservamos memoria para el nuevo nodo
  struct Nodo *nuevo = (struct Nodo *)malloc(sizeof(struct Nodo));
  if (nuevo == NULL) {
    pthread_mutex_unlock(&mutex);
    return -1; // error de memoria
  }

  // copiamos los datos de forma segura
  strncpy(nuevo->key, key, 255);
  nuevo->key[255] = '\0';

  strncpy(nuevo->value1, value1, 255);
  nuevo->value1[255] = '\0';

  nuevo->N_value2 = N_value2;
  for (int i = 0; i < N_value2; i++) {
    nuevo->V_value2[i] = V_value2[i];
  }

  nuevo->value3 = value3;
  nuevo->siguiente = cabeza;
  cabeza = nuevo;
  pthread_mutex_unlock(&mutex);
  return 0;
}

int get_value(char *key, char *value1, int *N_value2, float *V_value2,
              struct Paquete *value3) {
  if (key == NULL)
    return -1;
  pthread_mutex_lock(&mutex);
  struct Nodo *actual = cabeza;
  while (actual != NULL) {
    if (strcmp(actual->key, key) == 0) {
      // clave encontrada devolvemos los valores
      strcpy(value1, actual->value1);
      *N_value2 = actual->N_value2;
      for (int i = 0; i < actual->N_value2; i++) {
        V_value2[i] = actual->V_value2[i];
      }
      *value3 = actual->value3;

      pthread_mutex_unlock(&mutex);
      return 0;
    }
    actual = actual->siguiente;
  }
  pthread_mutex_unlock(&mutex);
  return -1; // eerror clave no encontrada
}

int modify_value(char *key, char *value1, int N_value2, float *V_value2,
                 struct Paquete value3) {
  if (N_value2 < 1 || N_value2 > 32)
    return -1;
  if (key == NULL || value1 == NULL)
    return -1;
  pthread_mutex_lock(&mutex);

  struct Nodo *actual = cabeza;
  while (actual != NULL) {
    if (strcmp(actual->key, key) == 0) {
      // clave encontrada actualizacion de valores
      strncpy(actual->value1, value1, 255);
      actual->value1[255] = '\0';

      actual->N_value2 = N_value2;
      for (int i = 0; i < N_value2; i++) {
        actual->V_value2[i] = V_value2[i];
      }
      actual->value3 = value3;

      pthread_mutex_unlock(&mutex);
      return 0;
    }
    actual = actual->siguiente;
  }
  pthread_mutex_unlock(&mutex);
  return -1; // error clave no encontrada
}

int delete_key(char *key) {
  if (key == NULL)
    return -1;

  pthread_mutex_lock(&mutex);

  struct Nodo *actual = cabeza;
  struct Nodo *anterior = NULL;

  while (actual != NULL) {
    if (strcmp(actual->key, key) == 0) {
      if (anterior == NULL) {
        cabeza = actual->siguiente;
      } else {
        // por si esta en el medio o al final
        anterior->siguiente = actual->siguiente;
      }
      free(actual); // liberamos la memoria del nodo

      pthread_mutex_unlock(&mutex);
      return 0;
    }
    anterior = actual;
    actual = actual->siguiente;
  }

  pthread_mutex_unlock(&mutex);
  return -1; // error clave no encontrada
}

int exist(char *key) {
  if (key == NULL)
    return -1;

  pthread_mutex_lock(&mutex);

  struct Nodo *actual = cabeza;
  while (actual != NULL) {
    if (strcmp(actual->key, key) == 0) {
      pthread_mutex_unlock(&mutex);
      return 1; // la clave existe
    }
    actual = actual->siguiente;
  }

  pthread_mutex_unlock(&mutex);
  return 0; // la clave no existe
}
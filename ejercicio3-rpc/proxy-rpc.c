/*
 * proxy-rpc.c  –  Implementacion de la biblioteca cliente (libproxyclaves.so)
 * que se comunica con el servidor mediante ONC RPC.
 *
 * La IP del servidor se lee de la variable de entorno IP_TUPLAS.
 * No es necesario conocer el puerto: el portmapper de RPC lo gestiona.
 *
 * Esta biblioteca implementa exactamente la misma interfaz publica
 * definida en claves.h, de forma que app-cliente.c no necesita
 * ninguna modificacion.
 */

#include "claves.h"
#include "clavesRPC.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Obtiene un handle CLIENT* al servidor indicado en IP_TUPLAS. */
static CLIENT *get_client(void)
{
    char *ip = getenv("IP_TUPLAS");
    if (ip == NULL) {
        fprintf(stderr, "ERROR: La variable de entorno IP_TUPLAS no esta definida.\n");
        return NULL;
    }

    CLIENT *clnt = clnt_create(ip, CLAVES_PROG, CLAVES_VERS, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(ip);
        return NULL;
    }
    return clnt;
}

/* ------------------------------------------------------------------ */

int destroy(void)
{
    CLIENT *clnt = get_client();
    if (clnt == NULL) return -1;

    int result = -1;
    enum clnt_stat st = destroy_1(&result, clnt);
    clnt_destroy(clnt);

    if (st != RPC_SUCCESS) return -1;
    return result;
}

int set_value(char *key, char *value1, int N_value2,
              float *V_value2, struct Paquete value3)
{
    if (N_value2 < 1 || N_value2 > 32) return -1;
    if (key == NULL || value1 == NULL)  return -1;

    CLIENT *clnt = get_client();
    if (clnt == NULL) return -1;

    SetArgs args;
    args.key    = key;
    args.value1 = value1;
    args.N_value2 = N_value2;
    args.V_value2.V_value2_len = (u_int)N_value2;
    args.V_value2.V_value2_val = V_value2;
    args.value3.x = value3.x;
    args.value3.y = value3.y;
    args.value3.z = value3.z;

    int result = -1;
    enum clnt_stat st = set_value_1(args, &result, clnt);
    clnt_destroy(clnt);

    if (st != RPC_SUCCESS) return -1;
    return result;
}

int get_value(char *key, char *value1, int *N_value2,
              float *V_value2, struct Paquete *value3)
{
    if (key == NULL) return -1;

    CLIENT *clnt = get_client();
    if (clnt == NULL) return -1;

    KeyArg   karg;
    karg.key = key;

    GetResult res;
    memset(&res, 0, sizeof(res));

    enum clnt_stat st = get_value_1(karg, &res, clnt);
    clnt_destroy(clnt);

    if (st != RPC_SUCCESS) return -1;

    int ret = res.ret;
    if (ret == 0) {
        strncpy(value1, res.value1, 255);
        value1[255] = '\0';

        *N_value2 = res.N_value2;
        for (int i = 0; i < res.N_value2; i++)
            V_value2[i] = res.V_value2.V_value2_val[i];

        value3->x = res.value3.x;
        value3->y = res.value3.y;
        value3->z = res.value3.z;
    }

    /* Liberamos la memoria XDR del resultado */
    xdr_free((xdrproc_t)xdr_GetResult, (caddr_t)&res);

    return ret;
}

int modify_value(char *key, char *value1, int N_value2,
                 float *V_value2, struct Paquete value3)
{
    if (N_value2 < 1 || N_value2 > 32) return -1;
    if (key == NULL || value1 == NULL)  return -1;

    CLIENT *clnt = get_client();
    if (clnt == NULL) return -1;

    SetArgs args;
    args.key    = key;
    args.value1 = value1;
    args.N_value2 = N_value2;
    args.V_value2.V_value2_len = (u_int)N_value2;
    args.V_value2.V_value2_val = V_value2;
    args.value3.x = value3.x;
    args.value3.y = value3.y;
    args.value3.z = value3.z;

    int result = -1;
    enum clnt_stat st = modify_value_1(args, &result, clnt);
    clnt_destroy(clnt);

    if (st != RPC_SUCCESS) return -1;
    return result;
}

int delete_key(char *key)
{
    if (key == NULL) return -1;

    CLIENT *clnt = get_client();
    if (clnt == NULL) return -1;

    KeyArg karg;
    karg.key = key;

    int result = -1;
    enum clnt_stat st = delete_key_1(karg, &result, clnt);
    clnt_destroy(clnt);

    if (st != RPC_SUCCESS) return -1;
    return result;
}

int exist(char *key)
{
    if (key == NULL) return -1;

    CLIENT *clnt = get_client();
    if (clnt == NULL) return -1;

    KeyArg karg;
    karg.key = key;

    int result = -1;
    enum clnt_stat st = exist_1(karg, &result, clnt);
    clnt_destroy(clnt);

    if (st != RPC_SUCCESS) return -1;
    return result;
}

/*
 * clavesRPC_server_impl.c  –  Implementacion de los procedimientos
 * remotos del servidor ONC RPC para el servicio de tuplas.
 *
 * Cada funcion _svc delega en la logica de almacenamiento de claves.c
 * (compilada como libclaves.so), que ya gestiona su propio mutex para
 * garantizar la concurrencia correcta.
 */

#include "clavesRPC.h"
#include "claves.h"
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------
 *  destroy
 * ------------------------------------------------------------------- */
bool_t
destroy_1_svc(int *result, struct svc_req *rqstp)
{
    (void)rqstp;
    *result = destroy();
    return TRUE;
}

/* -------------------------------------------------------------------
 *  set_value
 * ------------------------------------------------------------------- */
bool_t
set_value_1_svc(SetArgs arg1, int *result, struct svc_req *rqstp)
{
    (void)rqstp;
    struct Paquete p;
    p.x = arg1.value3.x;
    p.y = arg1.value3.y;
    p.z = arg1.value3.z;

    *result = set_value(
        arg1.key,
        arg1.value1,
        arg1.N_value2,
        arg1.V_value2.V_value2_val,
        p
    );
    return TRUE;
}

/* -------------------------------------------------------------------
 *  get_value
 * ------------------------------------------------------------------- */
bool_t
get_value_1_svc(KeyArg arg1, GetResult *result, struct svc_req *rqstp)
{
    (void)rqstp;
    char   value1[256];
    int    N_value2;
    float  V_value2[32];
    struct Paquete p;

    int ret = get_value(arg1.key, value1, &N_value2, V_value2, &p);

    result->ret = ret;

    if (ret == 0) {
        /* Copiamos el string value1 (XDR libera con xdr_free) */
        result->value1 = strdup(value1);
        result->N_value2 = N_value2;

        /* Reservamos y copiamos el vector de floats */
        result->V_value2.V_value2_len = (u_int)N_value2;
        result->V_value2.V_value2_val = (float *)malloc(N_value2 * sizeof(float));
        if (result->V_value2.V_value2_val != NULL) {
            for (int i = 0; i < N_value2; i++)
                result->V_value2.V_value2_val[i] = V_value2[i];
        }

        result->value3.x = p.x;
        result->value3.y = p.y;
        result->value3.z = p.z;
    } else {
        /* Valores vacios para evitar punteros colgantes en XDR */
        result->value1 = strdup("");
        result->N_value2 = 0;
        result->V_value2.V_value2_len = 0;
        result->V_value2.V_value2_val = NULL;
        result->value3.x = 0;
        result->value3.y = 0;
        result->value3.z = 0;
    }

    return TRUE;
}

/* -------------------------------------------------------------------
 *  modify_value
 * ------------------------------------------------------------------- */
bool_t
modify_value_1_svc(SetArgs arg1, int *result, struct svc_req *rqstp)
{
    (void)rqstp;
    struct Paquete p;
    p.x = arg1.value3.x;
    p.y = arg1.value3.y;
    p.z = arg1.value3.z;

    *result = modify_value(
        arg1.key,
        arg1.value1,
        arg1.N_value2,
        arg1.V_value2.V_value2_val,
        p
    );
    return TRUE;
}

/* -------------------------------------------------------------------
 *  delete_key
 * ------------------------------------------------------------------- */
bool_t
delete_key_1_svc(KeyArg arg1, int *result, struct svc_req *rqstp)
{
    (void)rqstp;
    *result = delete_key(arg1.key);
    return TRUE;
}

/* -------------------------------------------------------------------
 *  exist
 * ------------------------------------------------------------------- */
bool_t
exist_1_svc(KeyArg arg1, int *result, struct svc_req *rqstp)
{
    (void)rqstp;
    *result = exist(arg1.key);
    return TRUE;
}

/* -------------------------------------------------------------------
 *  Liberacion de resultados
 * ------------------------------------------------------------------- */
int
claves_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
    (void)transp;
    xdr_free(xdr_result, result);
    return 1;
}

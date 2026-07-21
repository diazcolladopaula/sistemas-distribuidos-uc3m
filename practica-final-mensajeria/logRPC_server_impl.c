/*
 * logRPC_server_impl.c - Implementación de los procedimientos RPC del
 * servicio de registro de operaciones.
 *
 * Cada vez que el servidor de mensajería realiza una operación, llama
 * a este servicio RPC que imprime:  Nombre_usuario   OPERACION
 */

#include "logRPC.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * log_op_1_svc: recibe LogArgs {username, operation} e imprime el log.
 */
bool_t
log_op_1_svc(LogArgs arg, int *result, struct svc_req *rqstp)
{
    (void)rqstp;
    /* Imprimir en formato: Nombre_usuario   OPERACION */
    printf("%s\t%s\n", arg.username, arg.operation);
    fflush(stdout);
    *result = 0;
    return TRUE;
}

int
log_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
    (void)transp;
    xdr_free(xdr_result, result);
    return 1;
}

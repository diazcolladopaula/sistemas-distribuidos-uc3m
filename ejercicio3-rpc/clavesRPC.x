/*
 * clavesRPC.x  –  Definicion de la interfaz ONC RPC para el servicio
 * de tuplas clave-valor1-valor2-valor3.
 *
 * Generacion de stubs:
 *   rpcgen -aNM clavesRPC.x
 */

/* ------------------------------------------------------------------ */
/*  Constantes de dominio                                              */
/* ------------------------------------------------------------------ */
const KEY_SIZE_RPC   = 256;
const VALUE1_SIZE_RPC = 256;
const MAX_V2_RPC     = 32;

/* ------------------------------------------------------------------ */
/*  Tipos de datos compartidos entre cliente y servidor               */
/* ------------------------------------------------------------------ */

/* Equivalente a struct Paquete de claves.h */
struct PaqueteRPC {
    int x;
    int y;
    int z;
};

/* Argumento de set_value y modify_value */
struct SetArgs {
    string key<KEY_SIZE_RPC>;
    string value1<VALUE1_SIZE_RPC>;
    int    N_value2;
    float  V_value2<MAX_V2_RPC>;
    PaqueteRPC value3;
};

/* Argumento para operaciones que solo necesitan la clave */
struct KeyArg {
    string key<KEY_SIZE_RPC>;
};

/* Resultado de get_value: codigo de retorno + datos */
struct GetResult {
    int    ret;           /* 0 exito, -1 error */
    string value1<VALUE1_SIZE_RPC>;
    int    N_value2;
    float  V_value2<MAX_V2_RPC>;
    PaqueteRPC value3;
};

/* ------------------------------------------------------------------ */
/*  Definicion del programa RPC                                        */
/* ------------------------------------------------------------------ */
program CLAVES_PROG {
    version CLAVES_VERS {
        /* destroy(): sin argumentos, devuelve int */
        int DESTROY(void) = 1;

        /* set_value(...): devuelve int */
        int SET_VALUE(SetArgs) = 2;

        /* get_value(...): devuelve GetResult */
        GetResult GET_VALUE(KeyArg) = 3;

        /* modify_value(...): devuelve int */
        int MODIFY_VALUE(SetArgs) = 4;

        /* delete_key(...): devuelve int */
        int DELETE_KEY(KeyArg) = 5;

        /* exist(...): devuelve int (1/0/-1) */
        int EXIST(KeyArg) = 6;

    } = 1;
} = 0x20001234;

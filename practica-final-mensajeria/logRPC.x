/*
 * logRPC.x - Interfaz ONC RPC del servicio de registro de operaciones
 * Sistemas Distribuidos - UC3M - Curso 2025-2026
 *
 * El servidor de mensajería (server.c) actúa como CLIENTE RPC.
 * El servidor RPC imprime: Nombre_usuario  OPERACION
 *
 * Justificación de la interfaz:
 *   - Se define un único procedimiento LOG_OP que recibe el nombre de usuario
 *     y la operación como cadenas. Es la interfaz más simple posible para
 *     el requisito descrito: solo se registra usuario + operación.
 *   - Para SENDATTACH la cadena operation vale "SENDATTACH /ruta/fichero".
 */

const LOG_USERNAME_MAX = 256;
const LOG_OPERATION_MAX = 512;

struct LogArgs {
    string username<LOG_USERNAME_MAX>;
    string operation<LOG_OPERATION_MAX>;
};

program LOG_PROG {
    version LOG_VERS {
        int LOG_OP(LogArgs) = 1;
    } = 1;
} = 0x20009999;

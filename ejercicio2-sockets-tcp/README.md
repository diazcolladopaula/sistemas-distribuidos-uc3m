# Ejercicio 2 — Servicio de almacenamiento sobre sockets TCP

Evolución directa del [Ejercicio 1](../ejercicio1-colas-mensajes): mismo servicio clave-valor, misma `claves.h`/`claves.c` sin tocar, pero la capa de comunicación pasa de colas POSIX a **sockets TCP** con un protocolo binario propio.

## Arquitectura

Igual que en el Ejercicio 1: `proxy-sock.c` (biblioteca cliente) y `servidor-sock.c` (servidor concurrente) sustituyen a `proxy-mq.c`/`servidor-mq.c`. `claves.c` y `app-cliente.c` se reutilizan sin cambios.

El servidor acepta conexiones TCP y lanza un **hilo detached por conexión** (`pthread_create` + `pthread_detach`), que atiende una única operación y cierra el socket al terminar.

## Protocolo de aplicación

Nada de volcar `struct`s de C directamente por el socket (no sería portable entre arquitecturas). Cada campo se serializa explícitamente en **big-endian** (`htonl`/`ntohl`):

| Tipo lógico | Representación en el cable |
|---|---|
| `int` | 4 bytes big-endian |
| `float` | 4 bytes IEEE 754 big-endian (bit-pattern por `htonl`) |
| Cadena (`key`/`value1`) | Campo fijo de 256 bytes, terminado en `\0` |

Toda petición empieza con un código de operación de 4 bytes. El cliente localiza el servidor mediante las variables de entorno `IP_TUPLAS` / `PORT_TUPLAS`, resueltas con `getaddrinfo()` (acepta tanto IPs como nombres de dominio).

## Compilación y ejecución

```bash
make
./servidor <PUERTO>
IP_TUPLAS=<IP> PORT_TUPLAS=<PUERTO> ./cliente_distribuido
```

El `Makefile` sigue el mismo esquema que el del Ejercicio 1 (mismos targets: `libclaves.so`, `libproxyclaves.so`, `servidor`, `cliente_local`, `cliente_distribuido`), adaptado a los ficheros `-sock` en vez de `-mq` y sin `-lrt` (los sockets no lo requieren).

## Pruebas

`app-cliente.c` ejecuta la misma batería de 8 casos que en el Ejercicio 1 contra el servidor real sobre TCP — comportamiento funcionalmente idéntico, solo cambia el transporte.

## Nota sobre el logging del servidor

Igual que en el Ejercicio 1, `servidor-sock.c` fuerza `stdout` a modo línea-por-línea (`setvbuf(stdout, NULL, _IOLBF, 0)`) en `main()` para que el log de operaciones no se pierda si el proceso se termina de forma abrupta.

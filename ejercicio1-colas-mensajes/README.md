# Ejercicio 1 — Servicio de almacenamiento sobre colas de mensajes POSIX

Servicio clave-valor `<key, value1, value2, value3>` con dos variantes: una **local** (biblioteca compartida) y otra **distribuida cliente-servidor** sobre colas de mensajes POSIX (`mqueue.h`), comunicadas mediante el patrón **Proxy**.

## Arquitectura

```
                    claves.h  (API pública, común a ambas variantes)
                        │
         ┌──────────────┴──────────────┐
         │                              │
   PARTE A (local)              PARTE B (distribuida)
         │                              │
    claves.c                      claves.c
   (lista enlazada +         (lista enlazada + mutex)
    mutex, libclaves.so)      corre DENTRO de servidor-mq.c
         │                              │
   app-cliente.c              proxy-mq.c (libproxyclaves.so)
   (enlaza libclaves.so            │
    directamente)             servidor-mq.c (multihilo, detached)
                                    │
                              app-cliente.c
                          (mismo código, enlaza
                           libproxyclaves.so en su lugar)
```

La clave del diseño: `app-cliente.c` es **exactamente el mismo fichero** en ambas variantes. Lo único que cambia es contra qué biblioteca se enlaza (`libclaves.so` directamente, o `libproxyclaves.so` que empaqueta las llamadas en mensajes y las envía al servidor). El cliente nunca sabe si está hablando con la lista enlazada en su propio proceso o con un servidor al otro lado de una cola de mensajes.

## Protocolo (proxy ↔ servidor)

Cada llamada de la API (`set_value`, `get_value`...) se traduce a una `struct Mensaje` (definida en `mensajes.h`) que viaja por una cola POSIX pública (`/cola_servidor_claves`). Cada cliente crea además una cola **temporal y única** (nombrada con su PID + ID de hilo) donde espera la respuesta — esto permite que múltiples clientes usen el servicio simultáneamente sin cruzarse las respuestas.

El servidor atiende peticiones en un hilo `detached` por mensaje recibido, delegando en `claves.c` (que gestiona su propio mutex), de forma que puede seguir escuchando la cola principal mientras procesa peticiones anteriores.

## Compilación y ejecución

```bash
make                    # genera libclaves.so, libproxyclaves.so, servidor,
                        # cliente_local y cliente_distribuido
./servidor &            # terminal 1
./cliente_distribuido   # terminal 2 — ejecuta la batería de pruebas de app-cliente.c
```

Para limpiar colas que hayan quedado abiertas tras un fallo: `rm -f /dev/mqueue/cola_*`.

## Pruebas

`app-cliente.c` ejecuta una batería de 8 casos contra el servidor real: inicialización, inserción, existencia, lectura, modificación, borrado, rechazo local de `N_value2` fuera de rango, y rechazo de clave duplicada por el servidor.

## Nota sobre el logging del servidor

`servidor-mq.c` fuerza `stdout` a modo línea-por-línea (`setvbuf(stdout, NULL, _IOLBF, 0)`) al inicio de `main()`. Sin esto, al redirigir la salida a un fichero stdio usa buffering completo, y el log de operaciones (`[Servidor] Procesando operación...`) puede quedar sin escribir si el proceso se termina con `kill` en vez de `Ctrl+C`.

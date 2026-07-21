# Ejercicio 3 — Servicio de almacenamiento sobre ONC RPC

Tercera variante del mismo servicio clave-valor, ahora sobre **ONC RPC** (`rpcgen`), eliminando la necesidad de diseñar el protocolo de red a mano: XDR se encarga de la serialización y el *portmapper* de la localización del servicio.

## Arquitectura

`claves.h`/`claves.c` se reutilizan sin cambios (tercera vez). Se añade la definición de interfaz `clavesRPC.x`, de la que `rpcgen -aNM` genera automáticamente:

| Fichero generado | Contenido |
|---|---|
| `clavesRPC.h` | Tipos y prototipos |
| `clavesRPC_clnt.c` | Stubs de cliente |
| `clavesRPC_svc.c` | Dispatcher del servidor + `main()` (registra en UDP y TCP) |
| `clavesRPC_xdr.c` | Serialización XDR de `SetArgs`, `KeyArg`, `GetResult`, `PaqueteRPC` |

El código propio son solo dos ficheros: `clavesRPC_server_impl.c` (implementación real de los 6 procedimientos remotos, delegando en `libclaves.so`) y `proxy-rpc.c` (biblioteca cliente que implementa `claves.h` sobre los stubs RPC generados). `clavesRPC_client.c` y `clavesRPC_server.c` son las plantillas de ejemplo que genera `rpcgen`; **no se usan**, quedan sustituidas por los dos ficheros anteriores.

El cliente solo necesita la IP del servidor (`IP_TUPLAS`); el *portmapper* de RPC resuelve el puerto automáticamente — no hace falta indicarlo.

## Compilación y ejecución

```bash
sudo systemctl start rpcbind   # necesario antes de arrancar el servidor
make
./clavesRPC_server &
IP_TUPLAS=<IP> ./cliente_distribuido
```

## Pruebas

`app-cliente.c` ejecuta la misma batería de 8 casos que en los ejercicios 1 y 2, contra el servidor RPC real con `rpcbind` activo.

## Notas de compilación

El Makefile separa las flags de compilación en dos grupos: `CFLAGS` (`-Wall -Wextra`) para el código propio, y `CFLAGS_RPC`, más permisivo, para los ficheros que genera `rpcgen` (`clavesRPC_svc.c`, `clavesRPC_xdr.c`) — el mismo patrón usado en la Práctica Final. Esto evita que los warnings del stub autogenerado se mezclen con los del código escrito a mano.

En `clavesRPC_server_impl.c`, cada función `_svc` recibe un parámetro `rqstp` que la interfaz RPC exige pero que no se usa; se marca explícitamente con `(void)rqstp;` al inicio de cada función para evitar el warning de parámetro no usado, igual que en `logRPC_server_impl.c` de la Práctica Final.

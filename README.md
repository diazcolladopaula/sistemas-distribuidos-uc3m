# Sistemas Distribuidos — UC3M

Colección de prácticas de la asignatura *Sistemas Distribuidos* (Grado en Ingeniería Informática, UC3M), que implementan el **mismo servicio de almacenamiento clave-valor** sobre **tres mecanismos de comunicación distintos**, y una práctica final que construye un **servicio de mensajería completo** combinando varias de esas tecnologías.

## Evolución del proyecto

| # | Proyecto | Tecnología | Lenguaje(s) |
|---|----------|-----------|-------------|
| 1 | [`ejercicio1-colas-mensajes`](./ejercicio1-colas-mensajes) | Colas de mensajes POSIX (`mqueue.h`) | C |
| 2 | [`ejercicio2-sockets-tcp`](./ejercicio2-sockets-tcp) | Sockets TCP + protocolo binario propio | C |
| 3 | [`ejercicio3-rpc`](./ejercicio3-rpc) | ONC RPC (`rpcgen`) | C |
| 4 | [`practica-final-mensajeria`](./practica-final-mensajeria) | Sockets TCP + REST (Flask) + ONC RPC | C + Python |

Los tres primeros ejercicios resuelven **el mismo problema** — un servicio `<key, value1, value2, value3>` con operaciones `set_value`, `get_value`, `modify_value`, `delete_key`, `exist`, `destroy` — cambiando únicamente la capa de comunicación entre cliente y servidor, con la lógica de almacenamiento (`claves.c`: lista enlazada + mutex) intacta en los tres. Es una demostración directa de cómo el patrón **Proxy** permite intercambiar el transporte sin tocar ni la lógica de negocio ni el cliente final.

La práctica final da un paso más: un servicio de mensajería tipo WhatsApp simplificado, con servidor concurrente en C, cliente multihilo en Python, un microservicio REST de normalización de texto (Flask) y un servicio de auditoría de operaciones por RPC.

## Notas técnicas transversales

- **Logging de los servidores en C**: los tres servidores (`servidor-mq.c`, `servidor-sock.c`, `server.c`) fuerzan `stdout` a modo línea-por-línea (`setvbuf(stdout, NULL, _IOLBF, 0)`), de forma que el log de operaciones queda escrito en el momento aunque la salida esté redirigida a fichero, en vez de depender del buffering completo por defecto.
- **Compilación de código generado por `rpcgen`**: en el Ejercicio 3 y la Práctica Final, los ficheros generados automáticamente (`clavesRPC_svc.c`, `clavesRPC_xdr.c`, `logRPC_svc.c`, `logRPC_xdr.c`) se compilan con flags más permisivos (`CFLAGS_RPC`) que el código propio, para que los warnings de compilación reflejen solo el código escrito a mano.

## Estructura

Cada subcarpeta es autocontenida: tiene su propio `Makefile`, su propio README con protocolo/arquitectura/pruebas, y compila de forma independiente.

## Licencia

MIT — ver [LICENSE](./LICENSE).

# Práctica Final — Servicio de mensajería distribuido

Servicio de notificación de mensajes entre usuarios (una versión simplificada de WhatsApp) que integra tres tecnologías de comunicación distinta en un único sistema: **sockets TCP** para la mensajería principal, un **servicio web REST** (Flask) para normalizar mensajes, y **ONC RPC** para auditar operaciones.

## Componentes

| Componente | Lenguaje | Tecnología | Función |
|---|---|---|---|
| `server.c` | C | Sockets TCP, multihilo | Registro, conexión, envío y entrega de mensajes |
| `client.py` | Python | Sockets TCP, multihilo | Interfaz de usuario (shell de comandos) |
| `web_service.py` | Python (Flask) | REST | Normaliza espacios en blanco repetidos en los mensajes |
| `logRPC_server_impl.c` + `logRPC.x` | C | ONC RPC | Registra usuario+operación de cada acción del sistema |

## Arquitectura del servidor

`server.c` es un servidor concurrente **thread-per-connection**: cada conexión TCP entrante se atiende en un hilo `detached` independiente. El estado se mantiene en un array `User[MAX_USERS]` protegido por un mutex global (`users_mu`) para el array, más un mutex por usuario para su cola de mensajes pendientes.

- **REGISTER / UNREGISTER**: alta/baja en el array de usuarios.
- **CONNECT / DISCONNECT**: guarda IP:puerto del cliente (obtenidos de `accept()` + el puerto que el cliente indica en el mensaje), y entrega los mensajes pendientes al conectar.
- **SEND / SENDATTACH**: asigna un ID de mensaje (contador `unsigned int` por remitente, con wraparound gestionado), encola el mensaje en la cola del destinatario, y lo entrega inmediatamente si está conectado.
- **USERS**: devuelve la lista de usuarios conectados con formato `usuario :: IP :: puerto` (extensión de la Parte 2 sobre el protocolo base).

El servidor actúa a la vez como **cliente RPC**: cada operación completada llama a `rpc_log()`, que si `LOG_RPC_IP` está definida, notifica al servidor de auditoría (`logRPC_server`). Si la variable no está definida, la llamada se omite sin afectar al servicio.

## Cliente Python

Clase estática con estado compartido. Al ejecutar `CONNECT` arranca un **hilo de escucha** (`_listener_thread`) que acepta conexiones entrantes del servidor: mensajes nuevos, ACKs de entrega, y peticiones de descarga de fichero (`GETFILE`, extensión sobre el enunciado base). Antes de enviar `SEND`/`SENDATTACH`, el mensaje se normaliza llamando al servicio web (`localhost:8081/normalize`); si el servicio no responde, se envía el mensaje sin modificar (degradación controlada, no bloquea la funcionalidad principal).

## Compilación y ejecución

```bash
sudo apt install rpcbind libtirpc-dev
pip3 install flask requests

make                                    # genera ./server y ./logRPC_server

# Orden recomendado (ver también variables de entorno):
sudo systemctl start rpcbind
./logRPC_server                         # terminal 1
python3 web_service.py                  # terminal 2
export LOG_RPC_IP=127.0.0.1 && ./server -p 9000   # terminal 3
python3 client.py -s 127.0.0.1 -p 9000  # terminal 4, 5...
```

## Ejemplo de uso

Con los tres servicios levantados (RPC, Flask, servidor de mensajería), dos usuarios pueden registrarse, conectarse e intercambiar un mensaje:

```
REGISTER alice -> 0 (OK)
REGISTER bob   -> 0 (OK)
REGISTER alice -> 1 (rechazado, ya existe)
CONNECT bob    -> 0 (OK)
USERS          -> 1 conectado: bob :: 127.0.0.1 :: <puerto>
SEND alice->bob "hola mundo" -> code=0, id=1
  → bob recibe: SEND MESSAGE / alice / id=1 / "hola mundo"
```

El log de auditoría RPC (`logrpc.log`) registra cada operación con su usuario: `bob REGISTER`, `alice REGISTER`, `bob CONNECT`, `bob USERS`, `alice SEND`.

## Notas de implementación

**Logging del servidor.** `server.c` fuerza `stdout` a modo línea-por-línea (`setvbuf(stdout, NULL, _IOLBF, 0)`) al inicio de `main()`, para que las líneas de log (`s> REGISTER ... OK/FAIL`) queden escritas de inmediato en vez de depender del buffering completo por defecto al redirigir la salida a fichero.

**Compactación del array de usuarios en `UNREGISTER`.** `handle_unregister` desplaza cada `struct User` una posición (`users[i] = users[i+1]`), incluyendo su `pthread_mutex_t` interno, para mantener el array compacto tras una baja. Esto introduce una ventana de carrera teórica: un hilo que obtuvo un índice de usuario (`sidx`/`didx` en `handle_send_common`) antes de una baja concurrente podría, en teoría, referirse a un usuario distinto tras la compactación. Una implementación más robusta evitaría compactar por índice — por ejemplo, usando punteros estables o un identificador de generación por usuario.

**`client.py`**: en `_listener_thread`, al manejar `GET FILE`, la línea `(void_r) = requester` es una asignación sin efecto (no cumple la función de descartar el valor que sugiere el nombre); puede eliminarse sin cambiar el comportamiento.

## Conclusiones

La práctica integra sockets TCP (control total del protocolo binario), REST/Flask (desacoplamiento del preprocesamiento) y ONC RPC (notificación sin gestionar el transporte). El principal reto de diseño es la concurrencia en el servidor C: el mutex por usuario y el mutex global del array deben adquirirse en un orden coherente para evitar deadlocks.

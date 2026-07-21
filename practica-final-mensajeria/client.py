#!/usr/bin/env python3
"""
client.py - Cliente de mensajería
Sistemas Distribuidos - UC3M - Curso 2025-2026

Implementa: REGISTER, UNREGISTER, CONNECT, DISCONNECT, USERS,
            SEND, SENDATTACH, GETFILE, QUIT

El cliente es concurrente: un hilo principal envía operaciones al servidor
y un hilo de escucha recibe mensajes entrantes del servidor.

El servicio web de normalización se llama en SEND y SENDATTACH antes de
enviar el mensaje al servidor (localhost:8081/normalize).
"""

from enum import Enum
import argparse
import socket
import threading
import struct
import os
import requests   # pip install requests


# ─────────────────────────────────────────────────────────────────────────────
# Constantes
# ─────────────────────────────────────────────────────────────────────────────
WEB_SERVICE_URL = "http://localhost:8081/normalize"
LISTEN_PORT_RANGE = (10000, 60000)   # Rango para buscar puerto libre


# ─────────────────────────────────────────────────────────────────────────────
class client:
    """Cliente del servicio de mensajería."""

    # ── Códigos de retorno ────────────────────────────────────────────────────
    class RC(Enum):
        OK         = 0
        ERROR      = 1
        USER_ERROR = 2

    # ── Atributos de clase (estado global del cliente) ────────────────────────
    _server      = None          # IP del servidor de mensajería
    _port        = -1            # Puerto del servidor de mensajería
    _username    = None          # Usuario actualmente conectado
    _listen_sock = None          # Socket de escucha para mensajes entrantes
    _listen_port = -1            # Puerto en el que escuchamos
    _listen_thread = None        # Hilo de escucha
    _connected   = False         # True si hay un usuario conectado
    _stop_event  = threading.Event()  # Para parar el hilo de escucha

    # Caché de usuarios conectados: {username: (ip, port)}
    _users_cache = {}
    _users_cache_lock = threading.Lock()

    # ── Helpers de socket ─────────────────────────────────────────────────────

    @staticmethod
    def _send_str(sock, s):
        """Envía una cadena terminada en \\0."""
        data = s.encode('utf-8') + b'\x00'
        sock.sendall(data)

    @staticmethod
    def _recv_str(sock):
        """Recibe una cadena terminada en \\0."""
        buf = b''
        while True:
            c = sock.recv(1)
            if not c:
                raise ConnectionError("Connection closed")
            if c == b'\x00':
                return buf.decode('utf-8', errors='replace')
            buf += c

    @staticmethod
    def _recv_byte(sock):
        """Recibe 1 byte como entero."""
        data = sock.recv(1)
        if not data:
            raise ConnectionError("Connection closed")
        return data[0]

    @staticmethod
    def _connect_to_server():
        """Crea y devuelve un socket conectado al servidor."""
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((client._server, client._port))
        return s

    # ── Servicio web ──────────────────────────────────────────────────────────

    @staticmethod
    def _normalize_message(message):
        """Llama al servicio web para normalizar el mensaje."""
        try:
            resp = requests.post(WEB_SERVICE_URL,
                                 json={'message': message},
                                 timeout=2)
            if resp.status_code == 200:
                return resp.json().get('message', message)
        except Exception:
            pass  # Si el servicio web no está disponible, usar mensaje original
        return message

    # ── Puerto libre ──────────────────────────────────────────────────────────

    @staticmethod
    def _find_free_port():
        """Busca un puerto TCP libre en el rango definido."""
        for p in range(LISTEN_PORT_RANGE[0], LISTEN_PORT_RANGE[1]):
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                s.bind(('', p))
                s.close()
                return p
            except OSError:
                continue
        return -1

    # ── Hilo de escucha ───────────────────────────────────────────────────────

    @staticmethod
    def _listener_thread():
        """
        Hilo que acepta conexiones entrantes del servidor y las procesa.
        Maneja: SEND MESSAGE, SEND MESS ACK,
                SEND MESSAGE ATTACH, SEND MESS ATTACH ACK,
                GET FILE
        """
        client._listen_sock.settimeout(1.0)
        while not client._stop_event.is_set():
            try:
                conn, addr = client._listen_sock.accept()
            except socket.timeout:
                continue
            except Exception:
                break
            try:
                op = client._recv_str(conn)

                if op == "SEND MESSAGE":
                    # Mensaje de texto del servidor
                    sender = client._recv_str(conn)
                    msg_id = client._recv_str(conn)
                    text   = client._recv_str(conn)
                    print(f"\ns> MESSAGE {msg_id} FROM {sender}")
                    print(f"   {text}")
                    print("   END")
                    print("c> ", end='', flush=True)

                elif op == "SEND MESSAGE ATTACH":
                    # Mensaje con fichero adjunto
                    sender   = client._recv_str(conn)
                    msg_id   = client._recv_str(conn)
                    text     = client._recv_str(conn)
                    filename = client._recv_str(conn)
                    print(f"\nc> MESSAGE {msg_id} FROM {sender}")
                    print(f"   {text}")
                    print("   END")
                    print(f"   FILE {filename}")
                    print("c> ", end='', flush=True)

                elif op == "SEND MESS ACK":
                    # ACK de entrega de mensaje sin adjunto
                    msg_id = client._recv_str(conn)
                    print(f"\nc> SEND MESSAGE {msg_id} OK")
                    print("c> ", end='', flush=True)

                elif op == "SEND MESS ATTACH ACK":
                    # ACK de entrega con adjunto
                    msg_id   = client._recv_str(conn)
                    filename = client._recv_str(conn)
                    print(f"\nc> SENDATTACH MESSAGE {msg_id} {filename} OK")
                    print("c> ", end='', flush=True)

                elif op == "GET FILE":
                    # Petición de transferencia de fichero
                    requester = client._recv_str(conn)
                    filename  = client._recv_str(conn)
                    (void_r)  = requester  # noqa: usado para debug
                    try:
                        with open(filename, 'rb') as f:
                            data = f.read()
                        # Enviar tamaño como 4 bytes big-endian, luego datos
                        conn.sendall(struct.pack('>I', len(data)))
                        conn.sendall(data)
                    except Exception:
                        conn.sendall(struct.pack('>I', 0))

            except Exception:
                pass
            finally:
                conn.close()

        try:
            client._listen_sock.close()
        except Exception:
            pass

    @staticmethod
    def _start_listener():
        """Arranca el hilo de escucha. Devuelve el puerto o -1."""
        port = client._find_free_port()
        if port < 0:
            return -1

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('', port))
        s.listen(32)
        client._listen_sock = s
        client._listen_port = port
        client._stop_event.clear()

        t = threading.Thread(target=client._listener_thread, daemon=True)
        t.start()
        client._listen_thread = t
        return port

    @staticmethod
    def _stop_listener():
        """Detiene el hilo de escucha."""
        client._stop_event.set()
        if client._listen_thread:
            client._listen_thread.join(timeout=3)
        client._listen_thread = None
        client._listen_sock   = None
        client._listen_port   = -1

    # ── Operaciones del protocolo ─────────────────────────────────────────────

    @staticmethod
    def register(user):
        """
        Registra al usuario en el servidor.
        Protocolo sección 8.1.
        """
        try:
            s = client._connect_to_server()
            client._send_str(s, "REGISTER")
            client._send_str(s, user)
            code = client._recv_byte(s)
            s.close()
        except Exception:
            print("c> REGISTER FAIL")
            return client.RC.ERROR

        if code == 0:
            print("c> REGISTER OK")
            return client.RC.OK
        elif code == 1:
            print("c> USERNAME IN USE")
            return client.RC.USER_ERROR
        else:
            print("c> REGISTER FAIL")
            return client.RC.ERROR

    @staticmethod
    def unregister(user):
        """
        Da de baja al usuario del servidor.
        Protocolo sección 8.2.
        """
        try:
            s = client._connect_to_server()
            client._send_str(s, "UNREGISTER")
            client._send_str(s, user)
            code = client._recv_byte(s)
            s.close()
        except Exception:
            print("c> UNREGISTER FAIL")
            return client.RC.ERROR

        if code == 0:
            print("c> UNREGISTER OK")
            return client.RC.OK
        elif code == 1:
            print("c> USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        else:
            print("c> UNREGISTER FAIL")
            return client.RC.ERROR

    @staticmethod
    def connect(user):
        """
        Conecta al usuario al servicio.
        Protocolo sección 8.3.
        Arranca el hilo de escucha antes de enviar la petición.
        """
        if client._connected:
            print("c> USER ALREADY CONNECTED")
            return client.RC.USER_ERROR

        port = client._start_listener()
        if port < 0:
            print("c> CONNECT FAIL")
            return client.RC.ERROR

        try:
            s = client._connect_to_server()
            client._send_str(s, "CONNECT")
            client._send_str(s, user)
            client._send_str(s, str(port))
            code = client._recv_byte(s)
            s.close()
        except Exception:
            client._stop_listener()
            print("c> CONNECT FAIL")
            return client.RC.ERROR

        if code == 0:
            client._connected = True
            client._username  = user
            print("c> CONNECT OK")
            return client.RC.OK
        elif code == 1:
            client._stop_listener()
            print("c> CONNECT FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif code == 2:
            client._stop_listener()
            print("c> USER ALREADY CONNECTED")
            return client.RC.USER_ERROR
        else:
            client._stop_listener()
            print("c> CONNECT FAIL")
            return client.RC.ERROR

    @staticmethod
    def disconnect(user):
        """
        Desconecta al usuario del servicio.
        Protocolo sección 8.4.
        """
        try:
            s = client._connect_to_server()
            client._send_str(s, "DISCONNECT")
            client._send_str(s, user)
            code = client._recv_byte(s)
            s.close()
        except Exception:
            client._stop_listener()
            client._connected = False
            client._username  = None
            print("c> DISCONNECT FAIL")
            return client.RC.ERROR

        # Siempre parar el hilo, incluso en error
        client._stop_listener()
        client._connected = False
        client._username  = None

        if code == 0:
            print("c> DISCONNECT OK")
            return client.RC.OK
        elif code == 1:
            print("c> DISCONNECT FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif code == 2:
            print("c> DISCONNECT FAIL, USER NOT CONNECTED")
            return client.RC.USER_ERROR
        else:
            print("c> DISCONNECT FAIL")
            return client.RC.ERROR

    @staticmethod
    def users():
        """
        Solicita la lista de usuarios conectados.
        Protocolo sección 8.7 (modificado Parte 2: devuelve usuario::IP::puerto).
        """
        if not client._connected or not client._username:
            print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED")
            return client.RC.USER_ERROR

        try:
            s = client._connect_to_server()
            client._send_str(s, "USERS")
            client._send_str(s, client._username)
            code = client._recv_byte(s)

            if code == 0:
                count_str = client._recv_str(s)
                count = int(count_str)
                print(f"c> CONNECTED USERS ({count} users connected) OK")
                with client._users_cache_lock:
                    client._users_cache.clear()
                for _ in range(count):
                    entry = client._recv_str(s)
                    print(f"   {entry}")
                    # Parsear "usuario :: IP :: puerto"
                    parts = [p.strip() for p in entry.split('::')]
                    if len(parts) == 3:
                        uname, uip, uport = parts
                        with client._users_cache_lock:
                            client._users_cache[uname] = (uip, int(uport))
                s.close()
                return client.RC.OK
            elif code == 1:
                s.close()
                print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED")
                return client.RC.USER_ERROR
            else:
                s.close()
                print("c> CONNECTED USERS FAIL")
                return client.RC.ERROR
        except Exception:
            print("c> CONNECTED USERS FAIL")
            return client.RC.ERROR

    @staticmethod
    def send(user, message):
        """
        Envía un mensaje de texto a otro usuario.
        Protocolo sección 8.5.
        Llama al servicio web para normalizar el mensaje.
        """
        # Normalizar con servicio web
        message = client._normalize_message(message)
        # Truncar a 255 chars
        message = message[:255]

        try:
            s = client._connect_to_server()
            client._send_str(s, "SEND")
            client._send_str(s, client._username if client._username else "")
            client._send_str(s, user)
            client._send_str(s, message)
            code = client._recv_byte(s)

            if code == 0:
                msg_id = client._recv_str(s)
                s.close()
                print(f"c> SEND OK - MESSAGE {msg_id}")
                return client.RC.OK
            elif code == 1:
                s.close()
                print("c> SEND FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                s.close()
                print("c> SEND FAIL")
                return client.RC.ERROR
        except Exception:
            print("c> SEND FAIL")
            return client.RC.ERROR

    @staticmethod
    def sendAttach(user, file, message):
        """
        Envía un mensaje con fichero adjunto.
        Protocolo sección 2.2 (Parte 2).
        Llama al servicio web para normalizar el mensaje.
        """
        # Normalizar con servicio web
        message = client._normalize_message(message)
        message = message[:255]

        try:
            s = client._connect_to_server()
            client._send_str(s, "SENDATTACH")
            client._send_str(s, client._username if client._username else "")
            client._send_str(s, user)
            client._send_str(s, message)
            client._send_str(s, file)
            code = client._recv_byte(s)

            if code == 0:
                msg_id = client._recv_str(s)
                s.close()
                print(f"c> SENDATTACH OK - MESSAGE {msg_id}")
                return client.RC.OK
            elif code == 1:
                s.close()
                print("c> SENDATTACH FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                s.close()
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR
        except Exception:
            print("c> SENDATTACH FAIL")
            return client.RC.ERROR

    @staticmethod
    def getFile(user, filename, local_filename):
        """
        Descarga un fichero desde otro usuario.
        Protocolo sección 2.5 (Parte 2).
        """
        # Buscar IP/puerto del usuario en caché
        with client._users_cache_lock:
            info = client._users_cache.get(user)

        if info is None:
            # Refrescar caché
            client.users()
            with client._users_cache_lock:
                info = client._users_cache.get(user)

        if info is None:
            print("c> FILE TRANSFER FAILED, user not connected.")
            return client.RC.USER_ERROR

        uip, uport = info
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((uip, uport))
            client._send_str(s, "GET FILE")
            client._send_str(s, client._username if client._username else "")
            client._send_str(s, filename)

            # Recibir tamaño (4 bytes) + datos
            size_bytes = b''
            while len(size_bytes) < 4:
                chunk = s.recv(4 - len(size_bytes))
                if not chunk:
                    raise ConnectionError("Connection closed")
                size_bytes += chunk
            size = struct.unpack('>I', size_bytes)[0]

            if size == 0:
                s.close()
                print("c> FILE TRANSFER FAILED, user not connected.")
                return client.RC.ERROR

            received = b''
            while len(received) < size:
                chunk = s.recv(min(4096, size - len(received)))
                if not chunk:
                    raise ConnectionError("Connection closed")
                received += chunk
            s.close()

            with open(local_filename, 'wb') as f:
                f.write(received)
            print(f"c> FILE {filename} DOWNLOADED TO {local_filename}")
            return client.RC.OK

        except Exception:
            print("c> FILE TRANSFER FAILED, user not connected.")
            return client.RC.ERROR

    # ── Shell ─────────────────────────────────────────────────────────────────

    @staticmethod
    def shell():
        """Intérprete de comandos del cliente."""
        while True:
            try:
                command = input("c> ")
                line = command.split(" ")
                if not line:
                    continue

                line[0] = line[0].upper()

                if line[0] == "REGISTER":
                    if len(line) == 2:
                        client.register(line[1])
                    else:
                        print("Syntax error. Usage: REGISTER <userName>")

                elif line[0] == "UNREGISTER":
                    if len(line) == 2:
                        client.unregister(line[1])
                    else:
                        print("Syntax error. Usage: UNREGISTER <userName>")

                elif line[0] == "CONNECT":
                    if len(line) == 2:
                        client.connect(line[1])
                    else:
                        print("Syntax error. Usage: CONNECT <userName>")

                elif line[0] == "DISCONNECT":
                    if len(line) == 2:
                        client.disconnect(line[1])
                    else:
                        print("Syntax error. Usage: DISCONNECT <userName>")

                elif line[0] == "USERS":
                    if len(line) == 1:
                        client.users()
                    else:
                        print("Syntax error. Usage: USERS")

                elif line[0] == "SEND":
                    if len(line) >= 3:
                        message = ' '.join(line[2:])
                        client.send(line[1], message)
                    else:
                        print("Syntax error. Usage: SEND <userName> <message>")

                elif line[0] == "SENDATTACH":
                    if len(line) >= 4:
                        message = ' '.join(line[3:])
                        client.sendAttach(line[1], line[2], message)
                    else:
                        print("Syntax error. Usage: SENDATTACH <userName> <filename> <message>")

                elif line[0] == "GETFILE":
                    if len(line) == 4:
                        client.getFile(line[1], line[2], line[3])
                    else:
                        print("Syntax error. Usage: GETFILE <userName> <fileName> <localFileName>")

                elif line[0] == "QUIT":
                    if len(line) == 1:
                        break
                    else:
                        print("Syntax error. Use: QUIT")
                else:
                    print(f"Error: command {line[0]} not valid.")

            except EOFError:
                break
            except Exception as e:
                print(f"Exception: {e}")

    @staticmethod
    def usage():
        print("Usage: python3 client.py -s <server> -p <port>")

    @staticmethod
    def parseArguments(argv):
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if args.s is None:
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False
        if args.p < 1024 or args.p > 65535:
            parser.error("Error: Port must be in range 1024 <= port <= 65535")
            return False

        client._server = args.s
        client._port   = args.p
        return True

    @staticmethod
    def main(argv):
        if not client.parseArguments(argv):
            client.usage()
            return
        client.shell()
        print("+++ FINISHED +++")


# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    client.main([])

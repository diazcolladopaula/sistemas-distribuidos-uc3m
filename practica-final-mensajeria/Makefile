CC      = gcc
CFLAGS  = -Wall -Wextra -g -I/usr/include/tirpc
# Flags más permisivos para los archivos generados por rpcgen
CFLAGS_RPC = -g -I/usr/include/tirpc -Wno-unused-variable -Wno-cast-function-type \
             -Wno-unused-parameter
LDFLAGS = -pthread -ltirpc

# ─── Todos los targets ────────────────────────────────────────────────────────
all: rpcgen_files server logRPC_server

# ─── Generar stubs RPC ────────────────────────────────────────────────────────
rpcgen_files: logRPC.x
	rm -f logRPC_server.c logRPC_client.c Makefile.logRPC
	rpcgen -aNM logRPC.x

# ─── Servidor de mensajería (Parte 1 + 2) ────────────────────────────────────
# Enlaza server.c con los stubs RPC del cliente (logRPC_clnt.c, logRPC_xdr.c)
server: server.c logRPC_clnt.c logRPC_xdr.c logRPC.h
	$(CC) $(CFLAGS) -c server.c -o server.o
	$(CC) $(CFLAGS_RPC) -c logRPC_clnt.c -o logRPC_clnt.o
	$(CC) $(CFLAGS_RPC) -c logRPC_xdr.c  -o logRPC_xdr.o
	$(CC) server.o logRPC_clnt.o logRPC_xdr.o -o server $(LDFLAGS)

# ─── Servidor RPC de log (Parte 3) ───────────────────────────────────────────
logRPC_server: logRPC_svc.c logRPC_xdr.c logRPC_server_impl.c logRPC.h
	$(CC) $(CFLAGS) -c logRPC_server_impl.c -o logRPC_server_impl.o
	$(CC) $(CFLAGS_RPC) -c logRPC_svc.c -o logRPC_svc.o
	$(CC) $(CFLAGS_RPC) -c logRPC_xdr.c  -o logRPC_xdr2.o
	$(CC) logRPC_svc.o logRPC_xdr2.o logRPC_server_impl.o -o logRPC_server $(LDFLAGS)

# ─── Limpieza ─────────────────────────────────────────────────────────────────
clean:
	rm -f *.o server logRPC_server
	rm -f logRPC.h logRPC_clnt.c logRPC_svc.c logRPC_xdr.c \
	      logRPC_server.c logRPC_client.c Makefile.logRPC

.PHONY: all clean rpcgen_files

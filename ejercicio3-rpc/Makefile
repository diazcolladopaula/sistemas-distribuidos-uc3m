CC      = gcc
CFLAGS  = -Wall -Wextra -g -I/usr/include/tirpc
# Flags mas permisivos para los archivos generados por rpcgen (no se deben editar)
CFLAGS_RPC = -g -I/usr/include/tirpc -Wno-unused-variable -Wno-cast-function-type \
             -Wno-unused-parameter
LDFLAGS = -pthread -ltirpc

all: rpcgen_files libclaves.so libproxyclaves.so clavesRPC_server cliente_distribuido

# -----------------------------------------------------------------------
#  Regla para invocar rpcgen y generar los stubs necesarios
# -----------------------------------------------------------------------
rpcgen_files: clavesRPC.x
	rm -f clavesRPC_server.c clavesRPC_client.c Makefile.clavesRPC
	rpcgen -aNM clavesRPC.x

# -----------------------------------------------------------------------
#  Biblioteca de almacenamiento (lado servidor)
# -----------------------------------------------------------------------
libclaves.so: claves.c claves.h
	$(CC) $(CFLAGS) -fPIC -c claves.c -o claves.o
	$(CC) -shared -o libclaves.so claves.o $(LDFLAGS)

# -----------------------------------------------------------------------
#  Ejecutable del servidor RPC
#  Combina: clavesRPC_svc.c (dispatcher generado), clavesRPC_xdr.c (XDR),
#           y clavesRPC_server_impl.c (implementacion de los procedimientos)
#  Enlaza con libclaves.so para el almacenamiento.
# -----------------------------------------------------------------------
clavesRPC_server: clavesRPC_svc.c clavesRPC_xdr.c clavesRPC_server_impl.c libclaves.so
	$(CC) $(CFLAGS) -c clavesRPC_server_impl.c -o clavesRPC_server_impl.o
	$(CC) $(CFLAGS_RPC) -c clavesRPC_svc.c -o clavesRPC_svc.o
	$(CC) $(CFLAGS_RPC) -c clavesRPC_xdr.c  -o clavesRPC_xdr.o
	$(CC) clavesRPC_svc.o clavesRPC_xdr.o clavesRPC_server_impl.o \
	    -o clavesRPC_server \
	    -L. -lclaves -Wl,-rpath=. \
	    $(LDFLAGS)

# -----------------------------------------------------------------------
#  Biblioteca proxy del cliente (libproxyclaves.so)
#  Combina: proxy-rpc.c + clavesRPC_clnt.c (stub RPC) + clavesRPC_xdr.c
# -----------------------------------------------------------------------
libproxyclaves.so: proxy-rpc.c clavesRPC_clnt.c clavesRPC_xdr.c claves.h clavesRPC.h
	$(CC) $(CFLAGS) -fPIC -c proxy-rpc.c     -o proxy-rpc.o
	$(CC) $(CFLAGS_RPC) -fPIC -c clavesRPC_clnt.c -o clavesRPC_clnt.o
	$(CC) $(CFLAGS_RPC) -fPIC -c clavesRPC_xdr.c  -o clavesRPC_xdr.o
	$(CC) -shared -o libproxyclaves.so \
	    proxy-rpc.o clavesRPC_clnt.o clavesRPC_xdr.o \
	    -ltirpc

# -----------------------------------------------------------------------
#  Ejecutable del cliente distribuido
# -----------------------------------------------------------------------
cliente_distribuido: app-cliente.c libproxyclaves.so
	$(CC) $(CFLAGS) app-cliente.c -o cliente_distribuido \
	    -L. -lproxyclaves -Wl,-rpath=.

clean:
	rm -f *.o *.so clavesRPC_server cliente_distribuido
	rm -f clavesRPC.h clavesRPC_clnt.c clavesRPC_svc.c \
	      clavesRPC_xdr.c clavesRPC_client.c clavesRPC_server.c \
	      Makefile.clavesRPC

.PHONY: all clean rpcgen_files

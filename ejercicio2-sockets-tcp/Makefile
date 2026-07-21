CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = -pthread

all: libclaves.so libproxyclaves.so servidor cliente_local cliente_distribuido

#  PARTE A (Local, reutilizada del ejercicio 1)
libclaves.so: claves.c claves.h
	$(CC) $(CFLAGS) -fPIC -c claves.c
	$(CC) -shared -o libclaves.so claves.o

cliente_local: app-cliente.c libclaves.so
	$(CC) $(CFLAGS) app-cliente.c -o cliente_local -L. -lclaves -Wl,-rpath=.

#  PARTE B (Distribuida sobre sockets TCP)
servidor: servidor-sock.c libclaves.so protocolo.h
	$(CC) $(CFLAGS) servidor-sock.c -o servidor -L. -lclaves -Wl,-rpath=. $(LDFLAGS)

libproxyclaves.so: proxy-sock.c claves.h protocolo.h
	$(CC) $(CFLAGS) -fPIC -c proxy-sock.c
	$(CC) -shared -o libproxyclaves.so proxy-sock.o $(LDFLAGS)

cliente_distribuido: app-cliente.c libproxyclaves.so
	$(CC) $(CFLAGS) app-cliente.c -o cliente_distribuido -L. -lproxyclaves -Wl,-rpath=.

clean:
	rm -f *.o *.so cliente_local cliente_distribuido servidor

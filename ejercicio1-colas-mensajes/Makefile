CC      = gcc
CFLAGS  = -Wall -Wextra -g
#-lrt para compilar mqueue y -pthread para los hilos
LDFLAGS = -lrt -pthread

all: libclaves.so libproxyclaves.so servidor cliente_local cliente_distribuido

#  PARTE A (Local)
libclaves.so: claves.c claves.h
	$(CC) $(CFLAGS) -fPIC -c claves.c
	$(CC) -shared -o libclaves.so claves.o

cliente_local: app-cliente.c libclaves.so
	$(CC) $(CFLAGS) app-cliente.c -o cliente_local -L. -lclaves -Wl,-rpath=.

#  PARTE B (Distribuida)
# el servidor usa libclaves.so
servidor: servidor-mq.c libclaves.so mensajes.h
	$(CC) $(CFLAGS) servidor-mq.c -o servidor -L. -lclaves -Wl,-rpath=. $(LDFLAGS)

# la libreria dinamica del proxy
libproxyclaves.so: proxy-mq.c claves.h mensajes.h
	$(CC) $(CFLAGS) -fPIC -c proxy-mq.c
	$(CC) -shared -o libproxyclaves.so proxy-mq.o $(LDFLAGS)

#el cliente distribuido usa el mismo codigo app cliente.c pero enlazado a libproxyclaves
cliente_distribuido: app-cliente.c libproxyclaves.so
	$(CC) $(CFLAGS) app-cliente.c -o cliente_distribuido -L. -lproxyclaves -Wl,-rpath=.

clean:
	rm -f *.o *.so cliente_local cliente_distribuido servidor

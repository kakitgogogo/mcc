all: httpd client
LIBS = -lpthread #-lsocket
httpd: httpd.c
	mcc $(LIBS) -o $@ $<

client: simpleclient.c
	mcc -o $@ $<
clean:
	rm httpd client

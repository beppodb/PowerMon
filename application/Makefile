#A very simple Makefile for PowerMon application.

CC=gcc
CFLAGS=-I.
OBJ = pm_application.o 

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

pm_app: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o pm_app

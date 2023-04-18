CC=gcc
CFLAGS=-I.
DEPS=fat_struct.h
OBJ=fat32.o

%.0: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fat32.o: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

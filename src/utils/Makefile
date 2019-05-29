SOURCE = $(wildcard *.c)
TARGETS = $(SOURCE:.c=.o)

CC = gcc
CFLAGS += -c -g -I../headers/

all: $(TARGETS)

*.o: *.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean

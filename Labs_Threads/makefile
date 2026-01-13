CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Wpedantic -fsanitize=address,undefined
LDFLAGS = -fsanitize=address,undefined
LDLIBS = -lpthread -lm

BIN = threads1 threads2 threads3 warehouse

.PHONY: all clean

all: $(BIN)

threads1: threads1.c
threads2: threads2.c
threads3: threads3.c
warehouse: warehouse.c
warehouse_adam: warehous_adam.c
clean:
	rm -f $(BIN) *.o

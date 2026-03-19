all: main
main: main.c	
	gcc -Wall -fsanitize=address,undefined -o main main.c
.PHONY: clean all
clean:
	rm main
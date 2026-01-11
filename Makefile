all: server

server: main.c l_system.c
	gcc -o server main.c l_system.c -lm -Wall

clean:
	rm -f server
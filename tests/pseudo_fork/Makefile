CFLAGS += -g
CFLAGS += -Wall
CFLAGS += -fno-stack-protector

all:
	gcc $(CFLAGS) -o main main.c setjmp.s longjmp.s -lpthread

tests:
	./main

clean:
	rm -f main

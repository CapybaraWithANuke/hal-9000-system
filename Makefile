all: Bowman Poole
CFLAGS =-Wall -Wextra -g

bowman.o: bowman.c
	gcc $(CFLAGS) -c bowman.c
poole.o: poole.c
	gcc $(CFLAGS) -c poole.c
Bowman: bowman.o
	gcc $(CFLAGS) bowman.o -o Bowman.out
Poole: poole.o
	gcc $(CFLAGS) poole.o -o Poole.out
	rm -f *.o

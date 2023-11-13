all: Bowman Poole Discovery
CFLAGS =-Wall -Wextra -g

bowman.o: bowman.c
	gcc $(CFLAGS) -c bowman.c
poole.o: poole.c
	gcc $(CFLAGS) -c poole.c
discovery.o: discovery.c
	gcc $(CFLAGS) -c discovery.c
Bowman: bowman.o
	gcc $(CFLAGS) bowman.o -o output/Bowman.out commonfuncs.c
Poole: poole.o
	gcc $(CFLAGS) poole.o -o output/Poole.out commonfuncs.c
Discovery: discovery.o
	gcc $(CFLAGS) discovery.o -o output/Discovery.out commonfuncs.c
	rm -f *.o
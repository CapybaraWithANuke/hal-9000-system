all: Bowman Discovery  #Poole 
CFLAGS =-Wall -Wextra -g

bowman.o: source/bowman.c
	gcc $(CFLAGS) -c source/bowman.c
#poole.o: source/poole.c
#	gcc $(CFLAGS) -c source/poole.c
discovery.o: source/discovery.c
	gcc $(CFLAGS) -c source/discovery.c
Bowman: bowman.o
	gcc $(CFLAGS) bowman.o -o output/Bowman.out source/commonfuncs.c
#Poole: poole.o
#	gcc $(CFLAGS) poole.o -o output/Poole.out source/commonfuncs.c
Discovery: discovery.o
	gcc $(CFLAGS) discovery.o -o output/Discovery.out source/commonfuncs.c
	rm -f *.o
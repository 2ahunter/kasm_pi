objects = knode.o crc_check.o timers.o

CFLAGS = -Wall -Wextra -pedantic -std=gnu17

LDLIBS = -lwiringPi


knode: $(objects)
	cc -o $@ $^ $(LDLIBS) # $@ is the target, $^ are the prerequisites

crc_check.o: crc_check.h

timers.o: timers.h

knode.o: crc_check.h timers.h

.PHONY : clean
clean :
	rm knode $(objects)
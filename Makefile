objects = knode.o crc_check.o timers.o

CFLAGS = -Wall -Wextra -pedantic -std=gnu17

LDLIBS = -lwiringPi


knode: $(objects)
	cc -o $@ $^ $(LDLIBS)

crc_check.o: crc_check.c crc_check.h

timers.o: timers.c timers.h

knode.o: knode.c crc_check.h timers.h

rtc_sim: rtc_sim.c UDP_client.o
	cc -o $@ $^ 

UDP_client.o: UDP_client.c UDP_client.h

.PHONY : clean
clean :
	rm knode $(objects)


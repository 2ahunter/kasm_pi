objects = UDP_client.o UDP_client_test.o

CFLAGS = -Wall -Wextra -pedantic -std=gnu17

UDP_test: $(objects)
	cc -o $@ $^ 

UDP_client_test.o: UDP_client_test.c UDP_client.h

knode: $(objects)
	cc -o $@ $^ $(LDLIBS)

knodeRT: knode_thr.o crc_check.o timers.o
	cc -o $@ $^ $(LDLIBS) -pthread

crc_check.o: crc_check.c crc_check.h

timers.o: timers.c timers.h

knode.o: knode.c crc_check.h timers.h

knode_thr.o: knode_thr.c knode_thr.h crc_check.h timers.h

rtc_sim: rtc_sim.c UDP_client.o
	cc -o $@ $^ 

UDP_client.o: UDP_client.c UDP_client.h

.PHONY : clean
clean :
	rm UDP_client $(objects)


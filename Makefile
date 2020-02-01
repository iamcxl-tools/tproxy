.PHONY: all clean

all:
	gcc -g main.c io_loop.c listener.c lock.c send_queue.c socket_context.c socket_utils.c sp.c bridge.c hashmap.c crc.c -lpthread -o tproxy

clean:
	-rm tproxy

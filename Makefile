CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -lrt

TARGETS = ipc_comms pipe_comms mq_comms shm_comms

all: $(TARGETS)

ipc_comms: ipc_comms.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

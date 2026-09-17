CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -lrt

TARGET = mq_comms
SRC = mq_comms.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

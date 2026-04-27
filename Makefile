CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -g
TARGET=scheduling_ships_rr

OBJS=main.o scheduler.o scheduler_rr.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c scheduler.h scheduler_rr.h
	$(CC) $(CFLAGS) -c main.c

scheduler.o: scheduler.c scheduler.h
	$(CC) $(CFLAGS) -c scheduler.c

scheduler_rr.o: scheduler_rr.c scheduler_rr.h scheduler.h
	$(CC) $(CFLAGS) -c scheduler_rr.c

run: all
	./$(TARGET)

clean:
	rm -f *.o $(TARGET)

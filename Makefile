CC=gcc
CFLAGS=-Wall -Wextra -std=c11
TARGET=scheduling_ships
OBJS=main.o scheduler.o scheduler_rr.o scheduler_priority.o scheduler_selector.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c scheduler.h scheduler_selector.h
	$(CC) $(CFLAGS) -c main.c

scheduler.o: scheduler.c scheduler.h
	$(CC) $(CFLAGS) -c scheduler.c

scheduler_rr.o: scheduler_rr.c scheduler_rr.h scheduler.h
	$(CC) $(CFLAGS) -c scheduler_rr.c

scheduler_priority.o: scheduler_priority.c scheduler_priority.h scheduler.h
	$(CC) $(CFLAGS) -c scheduler_priority.c

scheduler_selector.o: scheduler_selector.c scheduler_selector.h scheduler_rr.h scheduler_priority.h scheduler.h
	$(CC) $(CFLAGS) -c scheduler_selector.c

run-rr: $(TARGET)
	./$(TARGET) rr 2

run-priority: $(TARGET)
	./$(TARGET) priority

clean:
	rm -f $(TARGET) $(OBJS)

CC=gcc
CFLAGS=-Wall -Wextra -std=c11
TARGET=scheduling_ships
SRCS=main.c scheduler.c scheduler_rr.c scheduler_priority.c scheduler_sjf.c scheduler_strn.c scheduler_fcfs.c scheduler_selector.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

run-rr: $(TARGET)
	./$(TARGET) rr 2

run-priority: $(TARGET)
	./$(TARGET) priority

run-sjf: $(TARGET)
	./$(TARGET) sjf

run-strn: $(TARGET)
	./$(TARGET) strn

run-fcfs: $(TARGET)
	./$(TARGET) fcfs

clean:
	rm -f $(TARGET) *.o

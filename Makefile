CC=gcc
CFLAGS=-Wall -Wextra -std=c11
TARGET=scheduling_ships

# Se incluyen todos los módulos del proyecto para asegurar la compilación completa
SRCS=main.c scheduler.c scheduler_rr.c scheduler_priority.c scheduler_sjf.c scheduler_strn.c scheduler_fcfs.c scheduler_edf.c scheduler_selector.c canal.c ship_tasks.c ready_queue.c lcd_display.c scheduler_rr_freertos.c scheduler_priority_freertos.c scheduler_sjf_freertos.c scheduler_strn_freertos.c scheduler_fcfs_freertos.c scheduler_edf_freertos.c

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

run-edf: $(TARGET)
	./$(TARGET) edf

clean:
	rm -f $(TARGET) *.o
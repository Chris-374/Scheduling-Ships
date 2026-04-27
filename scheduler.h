#ifndef SCHEDULER_H
#define SCHEDULER_H

#define MAX_QUEUE_SIZE 4
#define NAME_SIZE 32

typedef enum {
    LEFT_SIDE = 0,
    RIGHT_SIDE = 1
} Side;

typedef enum {
    NORMAL = 0,
    FISHING = 1,
    PATROL = 2
} ShipType;

typedef struct {
    int id;
    char name[NAME_SIZE];
    ShipType type;
    Side side;
    int burst_time;
    int remaining_time;

    /* Campos usados por otros calendarizadores */
    int priority;
    int deadline;
} Task;

typedef struct Node {
    Task task;
    struct Node *next;
} Node;

typedef struct {
    Node *front;
    Node *rear;
    int size;
    int capacity;
    char name[NAME_SIZE];
} ReadyQueue;

Task createTask(int id, const char *name, ShipType type, Side side, int burst_time);
Task createTaskWithPriority(int id, const char *name, ShipType type, Side side, int burst_time, int priority);
void setTaskPriority(Task *task, int priority);

void initQueue(ReadyQueue *queue, const char *name, int capacity);
int isQueueEmpty(const ReadyQueue *queue);
int enqueue(ReadyQueue *queue, Task task);
int dequeue(ReadyQueue *queue, Task *task_out);
void printQueue(const ReadyQueue *queue);
void destroyQueue(ReadyQueue *queue);

const char *shipTypeToString(ShipType type);
const char *sideToString(Side side);

#endif

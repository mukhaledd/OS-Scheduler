#include <stdio.h> //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300

///==============================
// don't mess with this variable//
int *shmaddr; //
//===============================

int getClk()
{
    return *shmaddr;
}

int shmid;
/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
 */
void initClk()
{
    shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        // Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *)shmat(shmid, (void *)0, 0);
}

/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
 */

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        shmctl(shmid, IPC_RMID, NULL);
        killpg(getpgrp(), SIGINT);
    }
}

enum ProcessState
{
    ready,
    running,
    blocked,
    terminated
};

struct Process
{
    int id;
    int pid;
    int arrival_time;
    int runtime;
    int priority;
    int finishtime;
    int starttime;
    int waitingTime;
    int remainingTime;
    int lastStopTime;
    enum ProcessState state;
};

struct ProcessNode
{
    struct Process *process;
    struct ProcessNode *next;
};

union Semun
{
    int val;               /* Value for SETVAL */
    struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
    struct seminfo *__buf; /* Buffer for IPC_INFO (Linux-specific) */
};

void down(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

//-------------------------- RR headers ------------------------//

struct Queue
{
    struct ProcessNode *front;
    struct ProcessNode *rear;
};

void initializeQueue(struct Queue *q)
{
    q->front = NULL;
    q->rear = NULL;
}

void enqueue(struct Queue *q, struct Process *p)
{
    struct ProcessNode *newProcess;
    newProcess = (struct ProcessNode *)malloc(sizeof(struct ProcessNode));
    newProcess->process = p;
    newProcess->next = NULL;

    if (q->front == NULL)
    {
        // if empty queue
        q->front = newProcess;
        q->rear = newProcess;
    }
    else
    {
        (q->rear)->next = newProcess;
        q->rear = newProcess;
        (q->rear)->next = NULL;
    }
}

struct Process *dequeue(struct Queue *q)
{
    if (q->front != NULL)
    {
        struct ProcessNode *n = q->front;
        struct Process *p = (q->front)->process;

        if (q->front == q->rear)
        {
            q->front = NULL;
            q->rear = NULL;
        }
        else
        {
            q->front = (q->front)->next;
            n->next = NULL;
        }
        return p;
    }
    else
        return NULL;
}

void displayQ(struct Queue q)
{
    struct ProcessNode *it = q.front;
    if (!q.front)
        printf("the queue is empty");
    while (it != NULL && it != q.rear)
    {
        printf("processID:%d\n", (it->process)->id);
        it = it->next;
    }
    if (it != NULL)
        printf("processID:%d\n", (it->process)->id);
    return;
}

bool isEmpty(struct Queue q)
{
    if (q.front == NULL)
        return true;
    else
        return false;
}

// -----------------------------------------Priority Queue Data Structure ------------------------------

struct priority_Queue
{
    struct ProcessNode *front;
};

void initializePriorityQueue(struct priority_Queue *q)
{
    q->front = NULL;
}
// enqueue dequeue peek isempty display

void priority_enqueue(struct priority_Queue *q, struct Process *p, int op)
{
    struct ProcessNode *newProcess;
    newProcess = (struct ProcessNode *)malloc(sizeof(struct ProcessNode));
    newProcess->process = p;
    newProcess->next = NULL;
    if (op == 0)
    {
        if (q->front == NULL || q->front->process->priority > newProcess->process->priority)
        {
            newProcess->next = q->front;
            q->front = newProcess;
        }
        else
        {
            struct ProcessNode *temp = q->front;
            while (temp->next && temp->next->process->priority <= p->priority)
                temp = temp->next;
            newProcess->next = temp->next;
            temp->next = newProcess;
        }
    }
    if (op == 1)
    {
        if (q->front == NULL || q->front->process->remainingTime > newProcess->process->remainingTime)
        {
            newProcess->next = q->front;
            q->front = newProcess;
        }
        else
        {
            struct ProcessNode *temp = q->front;
            while (temp->next && temp->next->process->remainingTime <= p->remainingTime)
                temp = temp->next;
            newProcess->next = temp->next;
            temp->next = newProcess;
        }
    }
}

struct Process *priority_peek(struct priority_Queue *q)
{
    if (q->front)
        return q->front->process;
    else
        return NULL;
}

struct Process *priority_dequeue(struct priority_Queue *q)
{
    if (q->front)
    {
        struct Process *temp = q->front->process;
        struct ProcessNode *temp2 = q->front;
        q->front = q->front->next;
        temp2->next = NULL;

        return temp;
    }
    return NULL;
}

bool priority_isempty(struct priority_Queue *q)
{
    return (q->front == NULL);
}

void priority_display(struct priority_Queue *q)
{
    struct ProcessNode *it = q->front;

    while (it)
    {
        printf("process id = %d , arrival time = %d , priority = %d \n", it->process->priority, it->process->id, it->process->arrival_time);
        it = it->next;
    }
}

//------------------------------------------------------------------------

#ifndef QUEUE_H
#define QUEUE_H
#include <stdint.h>

typedef enum  {
    TASK_ACCEPT = 0,    // Task has connected but not yet received a RUN message
    TASK_BLOCKED,       // Task is blocked (waiting/IO wait)
    TASK_RUNNING,       // Task is in the ready queue or currently running
    TASK_STOPPED,       // Task has finished execution (sent DONE), waiting for more messages
    TASK_TERMINATED,    // Task has been terminated and will be removed
} task_status_en;

typedef struct task_st{
    int32_t pid;                   // Process ID (fake
    task_status_en  status;        // Current status of the task
    uint32_t time_ms;              // Time requested by application in milliseconds
    uint32_t ellapsed_time_ms;     // Time ellapsed since start in milliseconds
    uint32_t slice_start_ms;       // Time when the current time slice started
    uint32_t sockfd;               // Socket file descriptor for communication with the application
} task_t;

// Define singly linked list elements
typedef struct queue_elem_st queue_elem_t;
typedef struct queue_elem_st {
    task_t *task;
    queue_elem_t *next;
} queue_elem_t;

// Define the queue structure
// We define the head and the tail to make it easier to enqueue and dequeue
typedef struct queue_st  {
    queue_elem_t* head;
    queue_elem_t* tail;
} queue_t;

/**
 * @brief Create a new task
 *
 * This function allocates memory for a new task and initializes its fields.
 *
 * @param pid The process ID of the task
 * @param sockfd The socket file descriptor for communication with the application
 * @param time_ms a time field (either for run or block)
 * @return
 */
task_t *new_task(int32_t pid, uint32_t sockfd, uint32_t time_ms);

/**
 * @brief Enqueue a task into the queue
 *
 * This function adds a task to the end of the queue (FIFO order).
 *
 * @param q The queue to which the task will be added
 * @param task The task to be added to the queue
 * @return The number of tasks in the queue after adding the new task (0 on failure)
 */
int enqueue_task(queue_t* q, task_t* task);

/**
 * @brief Dequeue a task from the queue
 *
 * This function removes and returns the task at the front of the queue (FIFO order).
 *
 * @param q The queue from which the task will be removed
 * @return The task at the front of the queue, or NULL if the queue is empty
 */
task_t* dequeue_task(queue_t* q);

/**
 * @brief Remove a specific element from the queue
 *
 * This function removes a specific element from the queue.
 * Neither the element, nor the task inside the element, are freed.
 *
 * @param q The queue from which the element will be removed
 * @param elem The element to be removed from the queue
 * @return The removed element, or NULL if the element was not found
 */
queue_elem_t *remove_queue_elem(queue_t* q, queue_elem_t* elem);


#endif //QUEUE_H

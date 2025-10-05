#include "fifo.h"
#include <stdio.h>
#include <stdlib.h>
#include "msg.h"
#include <unistd.h>

#define NUM_QUEUES 3
#define TIME_SLICE_MLFQ 500

typedef struct {
    queue_t queues[NUM_QUEUES];
    int time_slices[NUM_QUEUES];
} mlfq_t;

// Track current task slice time and queue level
static mlfq_t mlfq;
static uint32_t current_slice_time = 0;
static int current_queue_level = 0;

void mlfq_init() {
    for (int i = 0; i < NUM_QUEUES; i++) {
        mlfq.queues[i].head = NULL;
        mlfq.queues[i].tail = NULL;
    }
    mlfq.time_slices[0] = 500;
    mlfq.time_slices[1] = 1000;
    mlfq.time_slices[2] = 2000;
}

void mlfq_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task, queue_t *command_queue) {
    static int initialized = 0;
    if (!initialized) {
        mlfq_init();
        initialized = 1;
    }

    // Handle currently running task
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;
        current_slice_time += TICKS_MS;

        // Check if task finished
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            // Return to command queue
            (*cpu_task)->status = TASK_COMMAND;
            (*cpu_task)->time_ms = 0;
            (*cpu_task)->ellapsed_time_ms = 0;
            enqueue_pcb(command_queue, *cpu_task);
            *cpu_task = NULL;
            current_slice_time = 0;
        }
        // Check if time slice expired
        else if (current_slice_time >= mlfq.time_slices[current_queue_level]) {
            // Demote to lower queue if possible
            // Demote to lower queue if possible
            int target_queue = current_queue_level;
            if (current_queue_level < NUM_QUEUES - 1) {
                target_queue++;
            }
            enqueue_pcb(&mlfq.queues[target_queue], *cpu_task);
            *cpu_task = NULL;
            current_slice_time = 0;
        }
    }

    // Move new tasks to Q0
    while (rq->head != NULL) {
        pcb_t *new_task = dequeue_pcb(rq);
        enqueue_pcb(&mlfq.queues[0], new_task);
    }

    // Find highest priority task
    if (*cpu_task == NULL) {
        for (int i = 0; i < NUM_QUEUES; i++) {
            if (mlfq.queues[i].head != NULL) {
                *cpu_task = dequeue_pcb(&mlfq.queues[i]);
                current_queue_level = i;
                current_slice_time = 0;
                break;
            }
        }
    }
}
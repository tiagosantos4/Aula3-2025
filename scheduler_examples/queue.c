#include "queue.h"

#include <stdio.h>
#include <stdlib.h>

task_t *new_task(pid_t pid, uint32_t sockfd, uint32_t time_ms) {
    task_t * new_task = malloc(sizeof(task_t));
    if (!new_task) return NULL;

    new_task->pid = pid;
    new_task->status = TASK_ACCEPT;
    new_task->slice_start_ms = 0;
    new_task->sockfd = sockfd;
    new_task->time_ms = time_ms;
    new_task->ellapsed_time_ms = 0;
    return new_task;
}

int enqueue_task(queue_t* q, task_t* task) {
    queue_elem_t* elem = malloc(sizeof(queue_elem_t));
    if (!elem) return 0;

    elem->task = task;
    elem->next = NULL;

    if (q->tail) {
        q->tail->next = elem;
    } else {
        q->head = elem;
    }
    q->tail = elem;
    return 1;
}

task_t* dequeue_task(queue_t* q) {
    if (!q || !q->head) return NULL;

    queue_elem_t* node = q->head;
    task_t* task = node->task;

    q->head = node->next;
    if (!q->head)
        q->tail = NULL;

    free(node);
    return task;
}

queue_elem_t *remove_queue_elem(queue_t* q, queue_elem_t* elem) {
    queue_elem_t* it = q->head;
    queue_elem_t* prev = NULL;
    while (it != NULL) {
        if (it == elem) {
            // Remove elem from queue
            if (prev) {
                prev->next = it->next;
            } else {
                q->head = it->next;
            }
            if (it == q->tail) {
                q->tail = prev;
            }
            return it;
        }
        prev = it;
        it = it->next;
    }
    printf("Queue element not found in queue\n");
    return NULL;
}
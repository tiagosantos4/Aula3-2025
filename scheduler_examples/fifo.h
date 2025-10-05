#ifndef FIFO_H
#define FIFO_H

#include "queue.h"

void fifo_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task);
void sjf_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task);
void rr_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task);
void mlfq_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task, queue_t *command_queue);

#endif // FIFO_H

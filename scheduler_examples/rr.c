#include "fifo.h"
#include <stdio.h>
#include <stdlib.h>
#include "msg.h"
#include <unistd.h>

#define TIME_SLICE 500  // 500ms conforme especificado

// Global variable to track current slice time
static uint32_t current_slice_remaining = 0;

/**
 * @brief Round Robin (RR) scheduling algorithm.
 * Executes tasks for a fixed time slice, then preempts if not finished.
 */
void rr_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;
        if (current_slice_remaining >= TICKS_MS) {
            current_slice_remaining -= TICKS_MS;
        } else {
            current_slice_remaining = 0;
        }

        // Task finished
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            free(*cpu_task);
            *cpu_task = NULL;
            current_slice_remaining = 0;
        }
        // Fatiamento expirou
        else if (current_slice_remaining == 0) {
            enqueue_pcb(rq, *cpu_task);
            *cpu_task = NULL;
            current_slice_remaining = 0;
        }
    }

    // Se CPU está livre, pega próximo da fila
    if (*cpu_task == NULL && rq->head != NULL) {
        *cpu_task = dequeue_pcb(rq);
        if (*cpu_task) {
            current_slice_remaining = TIME_SLICE;
        }
    }
}

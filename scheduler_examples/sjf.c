#include "fifo.h"
#include <stdio.h>
#include <stdlib.h>
#include "msg.h"
#include <unistd.h>

/**
 * @brief Shortest Job First (SJF) scheduling algorithm.
 * Selects the task with the shortest execution time from the ready queue.
 */
void sjf_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {
    // Atualiza tarefa em execução
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;

        // Se terminou
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
        }
    }

    // Se CPU está ociosa, escolhe o job mais curto
    if (*cpu_task == NULL && rq->head != NULL) {
        // Encontrar menor tempo na fila
        queue_elem_t *shortest = rq->head;
        for (queue_elem_t *current = rq->head->next; current != NULL; current = current->next) {
            uint32_t current_remaining = current->pcb->time_ms - current->pcb->ellapsed_time_ms;
            uint32_t shortest_remaining = shortest->pcb->time_ms - shortest->pcb->ellapsed_time_ms;
            if (current_remaining < shortest_remaining) {
                shortest = current;
            }
        }

        // Escalonar o mais curto
        *cpu_task = shortest->pcb;
        queue_elem_t *removed = remove_queue_elem(rq, shortest);
        if (removed) {
            free(removed);  // Libera o elemento da fila (NÃO o PCB!)
        }
    }
}

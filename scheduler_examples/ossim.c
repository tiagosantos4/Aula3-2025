#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "debug.h"

#define MAX_CLIENTS 128

#include <stdlib.h>
#include <sys/errno.h>

#include "fifo.h"
#include "msg.h"
#include "queue.h"

static uint32_t PID = 0;


/**
 * @brief Set up the server socket for the scheduler.
 *
 * This function creates a UNIX domain socket, binds it to a specified path,
 * and sets it to listen for incoming connections. It also sets the socket to
 * non-blocking mode.
 *
 * @param socket_path The path where the socket will be created
 * @return int Returns the server file descriptor on success, or -1 on failure
 */
int setup_server_socket(const char *socket_path) {
    int server_fd;
    struct sockaddr_un addr;

    // Clean up old socket file
    unlink(socket_path);

    // Create UNIX socket
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Bind
    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    // Listen
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    // Set the socket to non-blocking mode
    int flags = fcntl(server_fd, F_GETFL, 0); // Get current flags
    if (flags != -1) {
        if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl: set non-blocking");
        }
    }
    return server_fd;
}

/**
 * @brief Check for new client connections and add them to the queue.
 *
 * This function accepts new client connections on the server socket,
 * sets the client sockets to non-blocking mode, and enqueues them
 * into the provided queue.
 *
 * @param server_fd The server socket file descriptor
 * @param queue The queue to which new client tasks will be added
 */
void check_new_applications(int server_fd, queue_t *queue) {
    // Accept a new client connection
    int client_fd;
    do {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EMFILE || errno == ENFILE) {
                perror("accept: too many fds");
                break;
            }
            if (errno == EINTR)        continue;   // interrupted -> retry
            if (errno == ECONNABORTED) continue;   // aborted handshake -> next
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                perror("accept");
            }
            // No more clients to accept right now
            break;
        }
        int flags = fcntl(client_fd, F_GETFL, 0); // Get current flags
        if (flags != -1) {
            if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                perror("fcntl: set non-blocking");
            }
        }
        // Set close-on-exec flag
        int fdflags = fcntl(client_fd, F_GETFD, 0);
        if (fdflags != -1) {
            fcntl(client_fd, F_SETFD, fdflags | FD_CLOEXEC);
        }
        DBG("[Scheduler] New client connected: fd=%d\n", client_fd);
        // New tasks do not have a time yet, will be set when we receive a RUN message
        task_t *task = new_task(++PID, client_fd, 0);
        enqueue_task(queue, task);
    } while (client_fd > 0);
}

/**
 * @brief Check the wait queue for messages from clients.
 *
 * This function iterates through the wait queue, checking each client
 * socket for incoming messages. If a RUN message is received, the corresponding
 * task is moved to the ready queue and an ACK message is sent back to the client.
 * If a client disconnects or an error occurs, the client is removed from the wait queue.
 *
 * @param wait_queue The queue containing tasks waiting for CPU time
 * @param ready_queue The queue where tasks ready to run will be moved
 * @param current_time_ms The current time in milliseconds
 */
void check_wait_queue(queue_t * wait_queue, queue_t * ready_queue, uint32_t current_time_ms) {
    // Check all elements of the wait queue for new messages
    queue_elem_t * elem = wait_queue->head;
    while (elem != NULL) {
        task_t *current_task = elem->task;
        if (current_task->status == TASK_BLOCKED) {
            current_task->time_ms -= (current_task->time_ms>TICKS_MS)?TICKS_MS:current_task->time_ms;
            if (current_task->time_ms == 0) {
                current_task->status = TASK_STOPPED;
                // Send DONE message to the application
                msg_t msg = {
                    .pid = current_task->pid,
                    .request = PROCESS_REQUEST_DONE,
                    .time_ms = current_time_ms
                };
                if (write(current_task->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                    perror("write");
                }
                DBG("Process %d finished BLOCK, sending DONE\n", current_task->pid);
            }
        }
        msg_t msg;
        int n = read(current_task->sockfd, &msg, sizeof(msg_t));
        if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now, move to next
                elem = elem->next;
            } else {
                if (n < 0) {
                    perror("read");
                } else {
                    DBG("Connection closed by remote host\n");
                }
                // Remove from wait queue
                queue_elem_t *tmp = elem;
                elem = elem->next;
                free(current_task);
                free(tmp);
            }
            continue;
        }
        // We have received a message
        if (msg.request == PROCESS_REQUEST_BLOCK) {
            current_task->time_ms = msg.time_ms;
            current_task->status = TASK_BLOCKED;
            DBG("Process %d requested BLOCK for %d ms\n", current_task->pid);
        } else if (msg.request == PROCESS_REQUEST_RUN) {
            current_task->time_ms = msg.time_ms;
            current_task->status = TASK_RUNNING;
            enqueue_task(ready_queue, current_task);
            // Remove from wait queue
            remove_queue_elem(wait_queue, elem);
            queue_elem_t *tmp = elem;
            elem = elem->next;
            free(tmp);
            DBG("Process %d requested RUN for %d ms\n", current_task->pid, current_task->time_ms);
        } else {
            printf("Unexpected message received from client\n");
            continue;
        }
        // Send ack message
        msg_t ack_msg = {
            .pid = current_task->pid,
            .request = PROCESS_REQUEST_ACK,
            .time_ms = current_time_ms
        };
        if (write(current_task->sockfd, &ack_msg, sizeof(msg_t)) != sizeof(msg_t)) {
            perror("write");
        }
        DBG("Send ACK message to process %d with time %d\n", current_task->pid, current_time_ms);
    }
}

static const char *SCHEDULER_NAMES[] = {
    "FIFO",
    /*
    "SJB",
    "RR",
    "MLFQ",
    */
    NULL
};

typedef enum  {
    NULL_SCHEDULER = -1,
    SCHED_FIFO = 0,
    SCHED_SJB,
    SCHED_RR,
    SCHED_MLFQ
} scheduler_en;

scheduler_en get_scheduler(const char *name) {
    for (int i = 0; SCHEDULER_NAMES[i] != NULL; i++) {
        if (strcmp(name, SCHEDULER_NAMES[i]) == 0) {
            return (scheduler_en)i;
        }
    }
    printf("Scheduler %s not recognized. Available options are:\n", name);
    for (int i = 0; SCHEDULER_NAMES[i] != NULL; i++) {
        printf(" - %s\n", SCHEDULER_NAMES[i]);
    }
    return NULL_SCHEDULER;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <scheduler>\nScheduler options: FIFO", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments
    scheduler_en scheduler_type = get_scheduler(argv[1]);
    if (scheduler_type == NULL_SCHEDULER) {
        return EXIT_FAILURE;
    }

    // We set up two queues: a wait queue for tasks that have connected but not yet sent a RUN message,
    // and a ready queue for tasks that are ready to run.
    queue_t ready_queue = {.head = NULL, .tail = NULL};
    queue_t wait_queue = {.head = NULL, .tail = NULL};

    // We only have a single CPU that is a pointer to the actively running task on the CPU
    task_t *CPU = NULL;

    int server_fd = setup_server_socket(SOCKET_PATH);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to set up server socket\n");
        return 1;
    }
    printf("Scheduler server listening on %s...\n", SOCKET_PATH);
    uint32_t current_time_ms = 0;
    while (1) {
        // Check for new connections and set them to the wait queue
        check_new_applications(server_fd, &wait_queue);

        // Check for new messages from applications in the wait queue and move them to the ready queue
        check_wait_queue(&wait_queue, &ready_queue, current_time_ms);

        if (current_time_ms%1000 == 0) {
            printf("Current time: %d s\n", current_time_ms/1000);
        }

        fifo_scheduler(current_time_ms, &ready_queue, &CPU);

        // Simulate a tick
        usleep(TICKS_MS * 1000);
        current_time_ms += TICKS_MS;
    }

    return 0;
}

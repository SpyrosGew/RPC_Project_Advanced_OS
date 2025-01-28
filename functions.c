#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "functions.h"

typedef struct {
    int pipe_fd_to_child[2];
    int pipe_fd_to_parent[2];
    pid_t pid;
    char name[20];
    char message[100];
    sem_t *children_lock;
    int number;
} Child_info;

Child_info child_info;
int **children_pipes = NULL; //child read from these
int **parent_pipes = NULL;//parent read from these
sem_t **children_locks = NULL;
int fd = 0;
int children_amount = 0;
pid_t *child_pids = NULL;  // Array to store child PIDs

// SIGINT Handler: Terminates all children before exiting
void handle_sigint(int sig) {
    printf("\n[Parent] SIGINT received. Terminating all children...\n");
    for (int i = 0; i < children_amount; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);
        }
    }
    while (wait(NULL) > 0); // Wait for all children to terminate
    printf("[Parent] All children terminated. Exiting.\n");
    cleanup();
    exit(0);
}

// SIGTERM handler for child
void handle_sigterm(int sig) {
    printf("[Child %d] Terminating due to SIGTERM.\n", child_info.number);
    cleanup();
    exit(0);
}

void check_CLI_args(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Incorrect Command line arguments\n");
        exit(1);
    }
    if (strtol(argv[2], NULL, 10) < 1) {
        printf("Not enough processes specified\n");
        exit(1);
    }
}

void set_children_amount(int amount){
    if(amount < 1){
       printf("Error: No children to create.\n");
       cleanup();
       exit(1); 
    }
    children_amount = amount;
    child_pids = malloc(children_amount * sizeof(pid_t)); //child pids array for killing the children after signal
    if (!child_pids) {
        perror("Memory allocation failed");
        cleanup();
        exit(1);
    }
}

void initializeChildSemaphores() {
    children_locks = malloc(sizeof(sem_t *) * children_amount);
    if (!children_locks) {
        perror("Memory allocation failed");
        cleanup();
        exit(1);
    }
    for (int i = 0; i < children_amount; i++) {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "lock_of_child_%d", i);
        sem_unlink(sem_name);
        children_locks[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0644, (i == 0) ? 1 : 0);
        if (children_locks[i] == SEM_FAILED) {
            perror("Error creating semaphore");
            cleanup();
            exit(1);
        }
    }
}

void initialize_pipes() {
    children_pipes = malloc(children_amount * sizeof(int *));
    parent_pipes = malloc(children_amount * sizeof(int *));
    if (!children_pipes || !parent_pipes) {
        perror("Memory allocation failed for pipes");
        cleanup();
        exit(1);
    }
    for (int i = 0; i < children_amount; i++) {
        children_pipes[i] = malloc(2 * sizeof(int));
        parent_pipes[i] = malloc(2 * sizeof(int));
        if (!children_pipes[i] || !parent_pipes[i] ||
            pipe(children_pipes[i]) == -1 || pipe(parent_pipes[i]) == -1) {
            perror("Error creating pipes");
            cleanup();
            exit(1);
        }
    }
}

void file_opener(char* filename) {
    fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd == -1) {
        printf("There was an error opening the file\n");
        cleanup();
        exit(1);
    }
    printf("The file was created successfully\n");
    char parent_message [100];
    snprintf(parent_message, sizeof(parent_message), "[PARENT] -> <%d>\n", getpid());
    if (write(fd, parent_message, strlen(parent_message)) == -1){
        perror("Parent error writing to file");
        cleanup();
        exit(1);
    }
    fflush(stdout);
}

void send_message_to_child(int child_number){
    char message[100];
    snprintf(message, sizeof(message), "Hello child, I am your father and I call you: <child%d>", child_number);
    if (write(children_pipes[child_number][1], message, strlen(message)) == -1){
        perror("Error writing to pipe");
        cleanup();
        exit(1);
    }
}

int child_creation(){
    for(int child_number = 0; child_number < children_amount; child_number++){
        pid_t pid = fork();
        if(pid < 0){
            perror("Fork failed");
            cleanup();
            exit(1);
        }else if(pid == 0){
            signal(SIGTERM, handle_sigterm);
            return child_number; 
        }else{
            child_pids[child_number] = pid; 
            send_message_to_child(child_number); //this might be difficult b9 ut it outside the fork
        }           
    }
    return -1; //parent returns -1
}

void initialize_child_info(int child_number) {
    memcpy(child_info.pipe_fd_to_child, children_pipes[child_number], sizeof(child_info.pipe_fd_to_child));
    memcpy(child_info.pipe_fd_to_parent, parent_pipes[child_number], sizeof(child_info.pipe_fd_to_parent));
    child_info.pid = getpid();
    child_info.children_lock = children_locks[child_number];
    child_info.number = child_number;
    for (int i = 0; i < children_amount; i++) {
        if (i != child_number) {
            close(children_pipes[i][0]); // Close unused read end
            close(children_pipes[i][1]); // Close unused write end
            close(parent_pipes[i][1]);
            close(parent_pipes[i][0]);
        }
        free(children_pipes[i]); // Free pipe memory
        free(parent_pipes[i]);
    }
    free(children_locks);
    free(children_pipes); // Free the inhereted stuff
    free(parent_pipes);
    children_locks = NULL;
    children_pipes = NULL;
    parent_pipes = NULL;
}

 void child_loop(){
    while(1){
        char task_buffer[100];
        ssize_t bytes_read = read(child_info.pipe_fd_to_child[0], task_buffer, sizeof(task_buffer) - 1);
        if (bytes_read <= 0) {
            break;  // Exit loop on pipe close or error
        }
        task_buffer[bytes_read] = '\0';
        if (strcmp(task_buffer, "terminate") == 0) {
            printf("[Child %d] Terminating\n", child_info.number);
            break;
        }
        printf("[Child %d] Received task: %s\n", child_info.number, task_buffer);
        char result[100];
        snprintf(result, sizeof(result), "[Child %d] Completed: %s", child_info.number, task_buffer);
        // Truncation is acceptable in this context
        if (write(child_info.pipe_fd_to_parent[1], result, strlen(result)) == -1) {
            perror("Error writing result to parent");
            break;
        }
    }
 }

void parent_task_management(char** tasks, int task_count) {
    int remaining_tasks = task_count;
    int remaining_children = children_amount;
    int task_index = 0;
    fd_set readfds;
    int max_fd = 0;
    for (int i = 0; i < children_amount; i++) {
        if (parent_pipes[i][0] > max_fd) {
            max_fd = parent_pipes[i][0];
        }
    }
    while (remaining_tasks > 0 || remaining_children > 0) {
        FD_ZERO(&readfds);
        for (int i = 0; i < children_amount; i++) {
            FD_SET(parent_pipes[i][0], &readfds);
        }
        int result = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (result == -1) {
            perror("select failed");
            cleanup();
            exit(1);
        }
        for (int i = 0; i < children_amount; i++) {
            if (FD_ISSET(parent_pipes[i][0], &readfds)) {
                char buffer[100];
                ssize_t bytes_read = read(parent_pipes[i][0], buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    printf("Parent received: %s\n", buffer);
                    if (task_index < task_count) {
                        printf("Assigning new task to child %d: %s\n", i, tasks[task_index]);
                        if (write(children_pipes[i][1], tasks[task_index], strlen(tasks[task_index])) == -1) {
                            perror("Error writing task to child");
                        }
                        task_index++;
                    } else {
                        remaining_children--;
                        if (write(children_pipes[i][1], "terminate", 9) == -1) {
                            perror("Error sending termination signal");
                        }
                    }
                    remaining_tasks--;
                }
            }
        }
    }
}


void cleanup() {
    if (children_locks) {
        for (int i = 0; i < children_amount; i++) {
            if (children_locks[i]) {
                sem_close(children_locks[i]);
                char sem_name[20];
                snprintf(sem_name, sizeof(sem_name), "lock_of_child_%d", i);
                sem_unlink(sem_name);
            }
        }
        free(children_locks);
        children_locks = NULL;
    }
    if (children_pipes) {
        for (int i = 0; i < children_amount; i++) {
            if (children_pipes[i]) {
                close(children_pipes[i][0]); // Close read end
                close(children_pipes[i][1]); // Close write end
                free(children_pipes[i]);
            }
        }
        free(children_pipes);
        children_pipes = NULL;
    }
    if (parent_pipes) {
        for (int i = 0; i < children_amount; i++) {
            if (parent_pipes[i]) {
                close(parent_pipes[i][0]); // Close read end
                close(parent_pipes[i][1]); // Close write end
                free(parent_pipes[i]);
            }
        }
        free(parent_pipes);
        parent_pipes = NULL;
    }
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    printf("Resources cleaned up successfully.\n");
} 
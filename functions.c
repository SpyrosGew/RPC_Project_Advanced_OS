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
    sem_t *message_lock;
    int number;
} Child_info;

Child_info child_info;

int **children_pipes; //child read from these
int **parent_pipes; //parent read from these
sem_t **children_locks = NULL;
sem_t **message_locks = NULL;
int fd = 0;
int children_amount = 0;

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


void initialize_message_semaphores(){
    message_locks = malloc(sizeof(sem_t *) * children_amount);
    if(!message_locks){
        perror("Memory allocation failed");
        cleanup();
        exit(1);
    }
    for(int i = 0; i <children_amount; i++){
        char sem_name[35];
        snprintf(sem_name, sizeof(sem_name), "message_await_lock_child_%d", i);
        sem_unlink(sem_name);
        message_locks[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 0);
        if(message_locks[i] == SEM_FAILED){
            perror("Error creating semaphore");
            cleanup();
            exit(1);
        }
    }
}

void initialize_pipes() {
    children_pipes = malloc(children_amount * sizeof(int *));
    if (!children_pipes) {
        perror("Memory allocation failed for children_pipes");
        cleanup(); // Ensure cleanup if allocation fails
        exit(1);
    }
    for (int i = 0; i < children_amount; i++) {
        children_pipes[i] = malloc(2 * sizeof(int));
        if (!children_pipes[i] || pipe(children_pipes[i]) == -1) {
            perror("Error creating pipes");
            cleanup(); // Ensure cleanup if pipe creation fails
            exit(1);
        }
    }
    parent_pipes = malloc(children_amount * sizeof(int *));
    if (!parent_pipes) {
        perror("Memory allocation failed for parent_pipes");
        cleanup(); // Ensure cleanup if allocation fails
        exit(1);
    }
    for (int i = 0; i < children_amount; i++) {
        parent_pipes[i] = malloc(2 * sizeof(int));
        if (!parent_pipes[i] || pipe(parent_pipes[i]) == -1) {
            perror("Error creating pipes");
            cleanup(); // Ensure cleanup if pipe creation fails
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
    sem_post(message_locks[child_number]);
}

int child_creation(){
    for(int child_number = 0; child_number < children_amount; child_number++){
        pid_t pid = fork();
        if(pid < 0){
            perror("Fork failed");
            cleanup();
            exit(1);
        }else if(pid == 0){
            return child_number; 
        }else{ 
            send_message_to_child(child_number);
        }           
    }
    return -1; //parent returns -1
}

void initialize_child_info(int child_number) {
    memcpy(child_info.pipe_fd_to_child, children_pipes[child_number], sizeof(child_info.pipe_fd_to_child));
    memcpy(child_info.pipe_fd_to_parent, parent_pipes[child_number], sizeof(child_info.pipe_fd_to_parent));
    child_info.pid = getpid();
    child_info.children_lock = children_locks[child_number];
    child_info.message_lock = message_locks[child_number];
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
    free(children_pipes); // Free the inhereted stuff
    free(parent_pipes);
    free(message_locks);
    message_locks = NULL;
    children_pipes = NULL;
}

void update_child_info(){
    sem_wait(child_info.message_lock);
    char buffer[100];
    ssize_t bytes_read = read(child_info.pipe_fd_to_child[0], buffer, sizeof(buffer) -1);
    if (bytes_read > 0){
        buffer[bytes_read] = '\0';
        memcpy(child_info.message, buffer, sizeof(child_info.message));
    }else{
        perror("Error reading from pipe");
        cleanup();
        exit(1);
    }
    memcpy(child_info.name, strchr(child_info.message, '<'), sizeof(child_info.name));
}

void write_to_file() {
    sem_wait(child_info.children_lock);
    char message[100];
    snprintf(message, sizeof(message), "<%d> -> %s\n", child_info.pid, child_info.name);
    if(write(fd, message, strlen(message)) == -1){
        perror("Error writing to file");
        cleanup();
        exit(1);
    }
    notify_parent_done();
    if(child_info.number < children_amount - 1){
        sem_post(children_locks[child_info.number + 1]);
    }
}

void notify_parent_done() {
    const char* done_message = "done";
    if (write(child_info.pipe_fd_to_parent[1], done_message, strlen(done_message)) == -1) {
        perror("Error writing 'done' message to pipe");
        cleanup();
        exit(1);
    }
}


void wait_for_children_response() {
    fd_set readfds;  // Set of file descriptors to monitor
    int max_fd = 0;  // To track the highest file descriptor number

    for (int i = 0; i < children_amount; i++) {
        if (parent_pipes[i][0] > max_fd) {
            max_fd = parent_pipes[i][0];
        }
    }

    int remaining_children = children_amount;  // Track remaining children
    while (remaining_children > 0) {
        FD_ZERO(&readfds);
        for (int i = 0; i < children_amount; i++) {
            FD_SET(parent_pipes[i][0], &readfds);
        }

        int result = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (result == -1) {
            perror("select failed");
            exit(1);
        }

        for (int i = 0; i < children_amount; i++) {
            if (FD_ISSET(parent_pipes[i][0], &readfds)) {
                char buffer[100];
                ssize_t bytes_read = read(parent_pipes[i][0], buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';  // Null-terminate the string
                    printf("Child %d is done: %s\n", i, buffer);
                    remaining_children--;  // Decrease the number of remaining children
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
    if (message_locks) {
        for (int i = 0; i < children_amount; i++) {
            if (message_locks[i]) {
                sem_close(message_locks[i]);
                char sem_name[35];
                snprintf(sem_name, sizeof(sem_name), "message_await_lock_child_%d", i);
                sem_unlink(sem_name);
            }
        }
        free(message_locks);
        message_locks = NULL;
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
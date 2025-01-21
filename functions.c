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
    int pipe_fd[2];
    pid_t pid;
    char child_name[20];
} Child_info;

Child_info *children_info = NULL;
sem_t *parent_close_lock = NULL;
sem_t **children_locks = NULL;
int fd = 0;
int children_amount = 0;

void set_children_amount(int amount){
    children_amount = amount;
}

void initializeChildSemaphores() {
    children_locks = malloc(sizeof(sem_t *) * children_amount);
    if (!children_locks) {
        perror("Memory allocation failed");
        exit(1);
    }
    for (int i = 0; i < children_amount; i++) {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "lock_of_child_%d", i);
        children_locks[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0644, (i == 0) ? 1 : 0);
        if (children_locks[i] == SEM_FAILED) {
            perror("Error creating semaphore");
            cleanup();
            exit(1);
        }
    }
}

void initialize_parent_close_lock() {
    char sem_name[20];
    snprintf(sem_name, sizeof(sem_name), "parent_close_lock");
    parent_close_lock = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 0);
    if (parent_close_lock == SEM_FAILED) {
        perror("Error creating semaphore");
        cleanup();
        exit(1);
    }
}

void initialize_child_info(){
    children_info = malloc(sizeof(Child_info) * children_amount);
    if (!children_info){
        perror("Child memory allocation failed");
        cleanup();
        exit(1);
    }
}

void initialize_pipes(){
    for (int i = 0; i < children_amount; i++){
        if(pipe(children_info[i].pipe_fd) == -1){
            perror("Error creating pipes");
            cleanup();
            exit(1);
        }
    }
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

int child_creation(){
    for(int i = 0; i < children_amount; i++){
        pid_t pid = fork();
        if(pid < 0){
            perror("Fork failed");
            cleanup();
            exit(1);
        }else if(pid == 0){
            return i;  //return the child's number
        }else{ //child only reads also assosiation child with pipe
            close(children_info[i].pipe_fd[0]);
            children_info[i].pid = pid;
            snprintf(children_info[i].child_name, sizeof(children_info->child_name), "Child_%d", i);
        }           
    }
    return -1; //parent returns -1
}

void write_to_file(int child_number) {
    sem_wait(children_locks[child_number]);
    char message[100];
    snprintf(message, sizeof(message), "[CHILD%d] -> <%d>\n", child_number, getpid());
    if(write(fd, message, strlen(message)) == -1){
        perror("Error writing to file");
        cleanup();
        exit(1);
    }
    if(child_number < children_amount - 1){
        sem_post(children_locks[child_number + 1]);
    }
}

void wait_for_children() {
    for (int i = 0; i < children_amount; i++) {
        wait(NULL);
    }
}

void cleanup() {
    for (int i = 0; i < children_amount; i++) {
        char sem_name[20];
        snprintf(sem_name, sizeof(sem_name), "lock_of_child_%d", i);
        if (children_locks[i]) {
            sem_close(children_locks[i]);
            sem_unlink(sem_name);
        }
    }
    if(children_info){
        free(children_info);
    }
    if (children_locks) {
        free(children_locks);
    }
    if (parent_close_lock) {
        sem_close(parent_close_lock);
        sem_unlink("parent_close_lock");
    }
    if (fd != -1) {
        close(fd);
    }
    printf("Resources cleaned up successfully.\n");
}
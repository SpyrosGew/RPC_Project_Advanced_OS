#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

sem_t mutex;
int last_value = -1; // Indicates the last written process number
pid_t wpid, child_pid;
int status = 0;

void initialize() {
    sem_init(&mutex, 1, 1); // Initialize semaphore
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

int file_opener(char* file_name) {
    int fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        printf("There was an error opening the file\n");
        exit(1);
    }
    printf("The file was created successfully\n");
    fflush(stdout);
    return fd;
}

int process_initializer(long proc_amount) {
    for (int i = 0; i < proc_amount; i++) {
        if ((child_pid = fork()) < 0) {
            perror("fork");
            exit(1);
        } else if (child_pid > 0) {
            printf("Parent, forked child %d with pid = %d\n", i, child_pid);
        } else {
            printf("I am the %d child, with pid = %d\n", i, getpid());
            return i; // Child process returns its process number
        }
    }
    return -1; // Parent process returns -1
}

void write_to_file(int fd, int proc_num, pid_t pid) {
    while (1) {
        sem_wait(&mutex); // Wait for semaphore lock

        // Check if it's this process's turn to write
        if (last_value + 1 == proc_num || (proc_num == -1 && last_value == -1)) {
            char buffer[128];
            int len;
            if (proc_num == -1) {
                len = snprintf(buffer, sizeof(buffer), "Parent -> %d\n", pid);
            } else {
                len = snprintf(buffer, sizeof(buffer), "Child %d -> %d\n", proc_num, pid);
            }
            if (write(fd, buffer, len) == -1) {
                perror("Error writing to file");
            }
            last_value = proc_num; // Update the last written process
            sem_post(&mutex); // Release semaphore lock
            break; // Exit the loop after writing
        }

        // If not this process's turn, release the lock and yield
        sem_post(&mutex); // Release semaphore lock
    }
}

void parent_wait() {
  	while((wpid = wait(&status)) > 0);
    printf("Parent Process shuting down");
}
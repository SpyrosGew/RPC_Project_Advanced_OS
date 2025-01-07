//
// Created by spyrosgew on 13/12/2024.
//

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

void initialize();
void check_CLI_args(int argc, char* argv[]);
int process_initializer(long proc_amount);
int file_opener(char* file_name);
void write_to_file(int fd, int proc_num, pid_t pid);
void parent_wait();

#endif //FUNCTIONS_H

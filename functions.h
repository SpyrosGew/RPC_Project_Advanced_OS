//
// Created by spyrosgew on 13/12/2024.
//

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

void set_children_amount(int amount);
void initializeChildSemaphores();
void initialize_parent_close_lock();
void initialize_child_info();
void initialize_pipes();
void check_CLI_args(int argc, char* argv[]);
void file_opener(char* file_name);
int child_creation();
void write_to_file(int child_number);
void wait_for_children();
void cleanup();
#endif //FUNCTIONS_H
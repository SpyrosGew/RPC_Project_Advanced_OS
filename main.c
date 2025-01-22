#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "functions.h"

int main(int argc, char* argv[]) {
    check_CLI_args(argc, argv);

    int children_amount = (int) strtol(argv[2], NULL, 10);
    char* filename = argv[1];

    set_children_amount(children_amount);
    initializeChildSemaphores();
    initialize_message_semaphores();
    initialize_pipes();
    file_opener(filename);
    int child_number = child_creation();
    if (child_number == -1) {
        wait_for_children_response();
        cleanup();
    } else {
        initialize_child_info(child_number);
        update_child_info();
        write_to_file();
        exit(0); // Ensure child exits after writing
    }
    return 0;
}
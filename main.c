#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "functions.h"

int main(int argc, char* argv[]) {
    check_CLI_args(argc, argv);

    int child_amount = (int) strtol(argv[2], NULL, 10);
    char* filename = argv[1];
    
    initializeChildSemaphores(child_amount);
    initialize_parent_close_lock();
    file_opener(filename);
    int child_number = child_creation();
    if (child_number == -1) {
        wait_for_children();
        cleanup();
    } else {
        write_to_file(child_number);
        exit(0); // Ensure child exits after writing
    }
    return 0;
}

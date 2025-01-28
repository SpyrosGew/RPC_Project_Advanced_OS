#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "functions.h"

int main(int argc, char* argv[]) {
    check_CLI_args(argc, argv);

    int children_amount = (int) strtol(argv[2], NULL, 10);
    char* filename = argv[1];
    char* tasks[] = {
        "Task 1: Process data",
        "Task 2: Analyze input",
        "Task 3: Generate report",
        "Task 4: Send email"
    };

    int task_count = sizeof(tasks) / sizeof(tasks[0]);
    set_children_amount(children_amount);
    initializeChildSemaphores();
    initialize_pipes();
    file_opener(filename);
    int child_number = child_creation();
    if (child_number == -1) {
        parent_task_management(tasks, task_count);
        cleanup();
    } else {
        initialize_child_info(child_number);
        child_loop();
        cleanup();
        exit(0); // Ensure child exits after writing
    }
    return 0;
}
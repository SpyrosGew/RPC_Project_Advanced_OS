#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "functions.h"

int main(int argc, char* argv[]) {
    initialize();
    check_CLI_args(argc, argv);
    int proc_amount = (int) strtol(argv[2], NULL,10);
    int fd = file_opener(argv[1]);
    int proc_num = process_initializer(proc_amount);
    write_to_file(fd, proc_num, getpid());
    if (proc_num == -1) {
        parent_wait();
    }
    return 0;
}
## RPC PROJECT

FIRST STEP DONE commit : In this commmit the code can create n child processes and these processes can write into a file in order. NO parent child communication through pipes yet .

SECOND STEP DONE commit : In this commit the children wake up, they await for a message from the father , when the father sends a message the child (lets say child1) processes that message, writes to the file and unlocks the child 2 so it can also write to the file and in order.

gracefull shutdown commit : the final commit (sadly no time for RPC will maybe do it in the summer) here the writing in the file functionality is removed, but now the children can get more than 1 message (or task) and also the useless semaphores are removed working only with pipe block. Also the gracefull shutdown is implemented and we have no zombie processes on a term signal. Basically the parent simulates giving a file of rows of tasks and an available child can pick up one complete and return a message to the father. then then child can wait for another one. The programm ends normally when a all the tasks are finished and all resources are free'ed 

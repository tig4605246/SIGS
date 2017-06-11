/*

    Name: Xu Xi-Ping
    Date: March 8,2017
    Last Update: June 9,2017
    Program statement: 
        In here, We define the functions for controlling child processes.
        
*/




//Intent : Record process pid with .pid file
//Pre : Process Name
//Post : On success, return 0. Return -1 when fails

int sgsInitControl(char *processName);


//Intent : Release resources and terminated the calling process
//Pre : signal number
//Post : Nothing

void sgsChildAbort(int sigNum);
/*

    Name: Xu Xi-Ping
    Date: March 8,2017
    Last Update: March 8,2017
    Program statement: 
        In here, We define the functions for controlling child processes.
        
*/




//Intent : Record process pid with .pid file
//Pre : process Name
//Post : On success, return 0. Return -1 when fails

int sgsInitControl(char *processName);
/*

    Name: Xu Xi-Ping
    Date: March 8,2017
    Last Update: March 8,2017
    Program statement: 
        In here, We define the functions for controlling child processes.
        
*/

#include "SGSipcs.h"


//Intent : Free memories and exit the calling processes
//Pre : sigaction
//Post : Nothing, shows error when failed

void sgsStopSubProcess(struct sigaction *act);

//Intent : Define what the process should do when receiving SIGUSR1 signals
//Pre : Nothing
//Post : Nothing, shows error when failed

void sgsInitializeSigaction(void);

//Intent : initialize something for controlling. For example, store pid, initialize sigaction ... etc.
//Pre : Nothing
//Post : Nothing

void sgsInitializeSubProcess();
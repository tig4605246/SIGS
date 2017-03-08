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

void stopSubProcess(struct sigaction *act);

//Intent : Define what the process should do when receiving SIGUSR1 signals
//Pre : Nothing
//Post : Nothing, shows error when failed

void initializeSigaction(void);

//Intent : store deviceInfoPtr for stopSubProcess to use when freeing memories
//Pre :  Pointers of deviceInfoPtr and dataInfoPtr
//Post : On success, return 0. Otherwise return -1

int storeDeviceAndDataPtr(deviceInfo **deviceInfoPtr, dataInfo **dataInfoPtr);
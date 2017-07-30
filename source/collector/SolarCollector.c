/*

    Name: Xi-Ping Xu
    Date: July 19,2017
    Last Update: July 19,2017
    Program statement: 
        This is a agent used to test SGS system. 
        It has following functions:
        1. Register a data buffer to the data buffer pool
        2. Refresh data buffer every 30 seconds (including Inverter and irr meter)
        3. Receive, execute and return queue messages

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"
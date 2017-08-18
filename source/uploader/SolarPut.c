/*

    Name: Xi-Ping Xu
    Date: July 19,2017
    Last Update: August 18,2017
    Program statement: 
        This is an agent used to test SGS system. 
        It has following functions:
        1. Get a data buffer info from the data buffer pool
        2. Show fake data.
        3. Issue Command to FakeTaida and receive result

*/

/*

    Process:
    
    1. Init

    2. Send the data to server regarding to a certain period of the time passed in by the SolarPost

    3. Leaving

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


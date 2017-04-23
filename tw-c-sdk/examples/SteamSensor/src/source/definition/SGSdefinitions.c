/*

    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 10,2017
    Program statement: 
        we wrap ipcs at here, including: 
        shared memory
        message queue

*/

#include <stdio.h>
#include "SGSdefinitions.h"
//Intetnt : show current version
//Pre : Nothing
//Post : Nothing

void showVersion()
{

    printf("----\n");
    printf("| State : %s \t Version : %s\n",PROJECTSTATUS ,PROJECTVERSION);
    printf("----\n");

}
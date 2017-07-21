/*

    Name: Xu Xi-Ping
    Date: July 19,2017
    Last Update: July 19,2017
    Program statement: 
        A watchdog for the data buffer pool

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"

int DataBufferPoolId = -1;

void ShutDownBySignal(int sigNum);

int CheckPoolStatus(DataBufferInfo *ptr);

int main(int argc, char* argv[])
{

    int ret = -1;
    int id = -1;//  id for message queue to event-handler
    int dataBufferMasterId = -1;
    int i = 0;
    char buf[MSGBUFFSIZE];
    struct sigaction act, oldact; 

    printf("Child: SGSdatabuffermaster up\n");

    id = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);
    dataBufferMasterId = sgsCreateMsgQueue(DATABUFFER_SUBMASTER_KEY, 0);

    //Set signal action

    act.sa_handler = (__sighandler_t)ShutDownBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);
    
    //Ignore SIGINT
    
    signal(SIGINT, SIG_IGN);

    sgsSendQueueMsg(id, "[BOOT];DataBufferSubmaster", EnumDataBuffer);

    ret = sgsInitBufferPool(1);
    if(ret == -1)
    {

        printf("failed to open databuffer\n");
        exit(0);

    }
    DataBufferPoolId = ret;


    while(1)
    {

        //Routine check if the pool is working well or not

        for(i = 0 ; i < 50 ; i++)
        {

            ret = CheckPoolStatus(&(DataBufferInfoPtr[i]));
            if(ret == -1)
            {

                memset(buf,'\0',sizeof(buf));
                snprintf(buf,MSGBUFFSIZE - 1,"%s;Something's wrong with Pool number %d",ERROR,i);
                sgsSendQueueMsg(id, buf, EnumDataBuffer);

            }

        }
        sleep(10);

    }

}

void ShutDownBySignal(int sigNum)
{

    FILE *pf;
    pf=fopen("./fuck","w");
    fprintf(pf,"Fuck");
    fclose(pf);
    sgsDeleteBufferPool(DataBufferPoolId);
    printf("Buffer master bye bye\n");
    exit(0);

}

int CheckPoolStatus(DataBufferInfo *ptr)
{

    if(pthread_mutex_trylock( &(ptr->lock) ) != 0)
    {

        printf(LIGHT_RED"It's busy, check it next time\n"NONE);
        return 0;

    }
    else
    {

        if(ptr->inUse == 1)
        {
            if(ptr->shmId == -1)
            {

                printf("Something wrong with the BufferPool, dataName %s\n",ptr->dataName);
                return -1;

            }
        }
        else
        {

            if(ptr->shmId != -1)
            {

                printf("Something wrong with the BufferPool, dataName %s\n",ptr->dataName);
                return -1;

            }

        }
        pthread_mutex_unlock( &(ptr->lock));

    }
    return 0;

}
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>


#include "../protocol/SGSmodbus.h"
#include "../ipcs/SGSipcs.h"
#include "../controlling/SGScontrol.h"
#include "../events/SGSEvent.h"


dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;


//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();

//Intent : terminate the process correctly. It's called by the sigaction with signal SIGUSR1
//Pre : None
//Post : None

void stopAndAbort();

//Intent : Get tank Info
//Pre : dataInfo pointer
//Post : On success, return 0. On error, return -1 and shows the error message

int GetTankInfo(dataInfo *dataTemp);

int main(int argc, char *argv[])
{
    struct sigaction act, oldact;
    deviceInfo *temp = NULL;
    dataInfo *tmp = NULL;
    FILE *pidFile = NULL;
    int ret = 0;

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("[%s,%d] Failed to initialize the configure and shared memory, quitting\n",__FUNCTION__,__LINE__);

    }

    pidFile = fopen("./pid/FakeCollector.pid","w");
    if(pidFile != NULL)
    {
        fprintf(pidFile,"%d",getpid());
        fclose(pidFile);
    }
    

    act.sa_handler = (__sighandler_t)stopAndAbort;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

    temp = deviceInfoPtr;
    
    //Get the dataInfoPtr we want

    while(temp != NULL)
    {

        if(strcmp(temp->deviceName,"FakeCollector"))
            temp = temp->next;

        else
        {

            tmp = temp->dataInfoPtr;
            break;

        }

    }
    if(temp == NULL)
    {

        printf("Can't find GWInfo deviceInfo\n");
        return -1;

    }
    if(tmp == NULL)
    {

        printf("No dataInfo attached to the deviceInfo\n");
        return -1;

    }
    //Main Loop

    while(1)
    {

        tmp = temp->dataInfoPtr;
        
        GetTankInfo(tmp);

        sleep(10);
        

    }

    

    printf("\nWhatever FakeCollector INFO HERE\n");

    return 0;
}

int GetTankInfo(dataInfo *dataTemp)
{

    int ret = 0;
    dataLog dLog;

    while(dataTemp != NULL)
    {

        memset(dLog.value.s,0,sizeof(dLog.value.s));
        dLog.valueType = INTEGER_VALUE;
        dLog.value.i = rand() % 132070;
        ret = sgsWriteSharedMemory(dataTemp, &dLog);
        dataTemp = dataTemp->next;

    }

}

int initializeInfo()
{

    int ret = 0;
    ret = sgsInitDeviceInfo(&deviceInfoPtr);
    if(ret != 0)
    {

        printf("[%s,%d] init device conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    } 

    ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 0);
    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    return 0;

}

void stopAndAbort()
{

    sgsDeleteAll(deviceInfoPtr, -1);
    printf("[%s,%d] Catched SIGUSR1 successfully\n",__FUNCTION__,__LINE__);
    exit(0);

    return;

}
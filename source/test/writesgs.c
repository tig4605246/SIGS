/*
    Name: Xu Xi-Ping
    Date: April 20,2017
    Last Update: April 20,2017
    Program statement: 
        write back to shared memory and see if it's successful or not

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"

dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;

//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();


void main()
{

    dataInfo *ptr = NULL;
    dataLog dLog;
    deviceInfo *root = NULL;
    int i = 0;
    initializeInfo();

    root = deviceInfoPtr;

    while(strcmp(deviceInfoPtr->deviceName,"writesgs") && deviceInfoPtr != NULL)
        deviceInfoPtr = deviceInfoPtr->next;

    ptr = deviceInfoPtr->dataInfoPtr;

    while(ptr != NULL)
    {
        dLog.valueType = INTEGER_VALUE;
        dLog.value.i = i++;
        sgsWriteSharedMemory(ptr,&dLog);
        ptr = ptr->next;
    }
    printf("writesgs write done\n");
    sgsShowAll(root);
    printf("writesgs showAll done\n");
    return;

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


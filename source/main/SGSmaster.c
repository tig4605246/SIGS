/*
    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 8,2017
    Program statement: 
        This program will manage all other sub processes it creates.
        Also, it's responsible for opening and closing all sub processes.


    Program Flow:

        Read in config file about protocol
        Parse config file
        if(file is not ok)
            print "bad file"
            exit(0)
        close config file
        read daemon file (contains the names of agents)
        create linked list to store daemon message
        for (linked list is not the end)
            pid = fork()
            if(pid == 0)
                execlp daemon
            else if (pid == -1)
                print"open failed"
                mail(open "that" daemon failed)
            else
                print"open "that" daemon successfully"
                store pid in linked list
        
        while(1)
            if(input == 0)
                close all daemon
                exit(0)
            else if(input == 1)
                restart all deamon

*/

#include <stdio.h>
#include <stdlib.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"

//Store shared memory id

int shmID = 0;

dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;

//Intent : Modulize starting function
//Pre : dataInfoPtr, deviceInfoPtr 
//Post : Nothing

int initializeInfo();

//Intent : free deviceInfoPtr, dataInforPtr and free the shared memory

int releaseResource();

int main()
{

    int i, ret = 0;
    FILE *daemonFp = NULL;
    dataLog source;

    printf("Starting SGSmaster...\n");
    showVersion();



    ret = sgsInitDeviceInfo(&deviceInfoPtr);
    if(ret != 0) return -1;

    ret = sgsInitDataInfo(NULL, &dataInfoPtr, 1);
    if(ret == 0) return -1;

    shmID = ret;



    sgsShowDeviceInfo(deviceInfoPtr);

    source.valueType = STRING_VALUE;
    sprintf(source.value.s,"hi I'm doing my job here");

    sgsWriteSharedMemory(dataInfoPtr, &source);

    sgsShowDataInfo(dataInfoPtr);

    sgsDeleteDataInfo(dataInfoPtr,shmID);

    sgsDeleteDeviceInfo(deviceInfoPtr);

    



    return 0;
}
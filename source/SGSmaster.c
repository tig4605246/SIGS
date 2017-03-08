/*
    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 1,2017
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

#include "SGSipcs.h"


int main()
{

    int i, ret = 0;
    deviceInfo *deviceInfoPtr = NULL;
    dataInfo *dataInfoPtr = NULL;

    ret = sgsInitDeviceInfo(&deviceInfoPtr);
    if(ret != 0) return -1;

    sgsShowDeviceInfo(deviceInfoPtr);

    ret = sgsInitDataInfo(&deviceInfoPtr,1);
    if(ret == 0) return -1;

    sgsShowDataInfo(deviceInfoPtr->dataInfoPtr);

    sgsDeleteDataInfo(deviceInfoPtr->dataInfoPtr,ret);

    sgsDeleteDeviceInfo(deviceInfoPtr);

    



    return 0;
}
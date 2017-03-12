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
#include <signal.h>
#include <string.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"

//Store shared memory id

int shmID = 0;

dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;

//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();

//Intent : free deviceInfoPtr, dataInforPtr and free the shared memory (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error messages

void releaseResource();

//Intent : Start sub processes
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error messages

void startAllProcesses();

//Intent : Stop sub processes by using the pid stored in the deviceInfo struct
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error messages

void stopAllSubProcesses(struct sigaction *act);

//Intent : write data to shared memory
//Pre : Nothing
//Post : Nothing

void testWriteSharedMemory();

int main()
{

    int i, ret = 0;
    char input;
    FILE *daemonFp = NULL;
    struct sigaction act, oldact;
    
    printf("Starting SGSmaster...\n");
    showVersion();




    //sgsShowDeviceInfo(deviceInfoPtr);

    act.sa_handler = (__sighandler_t)stopAllSubProcesses;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGINT, &act, &oldact);

    printf("Starting sub processes...\n");
    //startAllProcesses();

    while(1)
    {

        printf("$-> ");
        scanf(" %c",&input);

        switch(input)
        {

            case 'l' :
                sgsShowDeviceInfo(deviceInfoPtr);
                sgsShowDataInfo(dataInfoPtr);
                break;

            case 'k' :
                stopAllSubProcesses(NULL);
                break;
            
            case 'f' :
                releaseResource();
                break;

            case 'r' :
                stopAllSubProcesses(NULL);
                releaseResource();
                initializeInfo();
                startAllProcesses();
                break;

            case 'x' :
                printf("bye\n");
                exit(0);
                break;

            default :
                printf("commands : \n");
                printf("l - list contents of the device conf and data conf\n");
                printf("k - kill all sub processes\n");
                printf("f - free all allocated resources and frees shared memory (do this after killing sub processes)\n");
                printf("r - Restart \n");
                printf("\n");
                break;

        }



    }
    

    



    return 0;
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

    ret = sgsInitDataInfo(NULL, &dataInfoPtr, 1);
    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    shmID = ret;

    return 0;

}

void releaseResource()
{

    sgsDeleteDataInfo(dataInfoPtr,shmID);

    sgsDeleteDeviceInfo(deviceInfoPtr);

    return ;

}

void startAllProcesses()
{

    pid_t pid = 0;
    char buf[128];
    deviceInfo *ptr = deviceInfoPtr;

    printf("[%s,%d] Starting sub processes...\n\n",__FUNCTION__,__LINE__);

    while(ptr != NULL)
    {

        //fork sub process

        pid = fork();

        //decide what to do by pid

        if(pid == 0)
        {

            //opening sub process

            memset(buf,'\0',sizeof(buf));
            sprintf(buf,"./%s",ptr->deviceName);
            printf("starting %s with pid %d\n\n",buf,pid);
            execlp(buf,buf,ptr->deviceName,NULL);

            //Only get here when execlp fails

            perror("execlp");
            exit(-1);

        }
        else if(pid > 0)
        {

            ptr->subProcessPid = pid;

        }
        else
        {

            //pid < 0, forking failed

            printf("[%s,%d] fork() return %d, forking %s failed, %s\n",__FUNCTION__, __LINE__, pid, ptr->deviceName, strerror(pid));

        }

        //next deviceInfo

        ptr = ptr->next;

    }
    return ;

}

void stopAllSubProcesses(struct sigaction *act)
{

    int ret = 0;
    pid_t pid = 0;
    char buf[128];
    deviceInfo *ptr = deviceInfoPtr;

    printf("[%s,%d] Stopping sub processes...\n\n",__FUNCTION__,__LINE__);

    while(ptr != NULL)
    {

        printf("[%s,%d] Stopping %s now (stored pid is %d) \n",__FUNCTION__,__LINE__,ptr->deviceName,ptr->subProcessPid);
        ret = kill(ptr->subProcessPid,SIGKILL);
        if(ret < 0)
        {

            printf("[%s,%d] kill failed %s\n",__FUNCTION__, __LINE__, strerror(ret));
            
        }
        ptr = ptr->next;

    }
    return;

}

void testWriteSharedMemory()
{

    dataLog source;

    source.valueType = STRING_VALUE;

    sprintf(source.value.s,"hi I'm doing my job here");
    sgsWriteSharedMemory(dataInfoPtr, &source);

    sgsShowDataInfo(dataInfoPtr);

    return ;

}
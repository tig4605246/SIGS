/*
    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 8,2017
    Program statement: 
        This program will manage all other sub processes it creates.
        Also, it's responsible for opening and closing all sub processes.

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

void startCollectingProcesses();

//Intent : Stop sub processes by using the pid stored in the deviceInfo struct
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error messages

void stopAllCollectingProcesses(int sigNum);

//Intent : write data to shared memory
//Pre : Nothing
//Post : Nothing

void testWriteSharedMemory();

//Intent : start GW-to-Server processes
//Pre : Nothing
//Post : Nothing

void stratUploadingProcesses();

//Intent : stop GW-to-Server processes
//Pre : Nothing
//Post : Nothing

void stopUploadingProcesses();


int main()
{

    int i, ret = 0;
    char input;
    FILE *daemonFp = NULL;
    struct sigaction act, oldact;
    
    
    printf("Starting SGSmaster...\n");
    showVersion();


    //sgsShowDeviceInfo(deviceInfoPtr);

    act.sa_handler = (__sighandler_t)stopAllCollectingProcesses;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGINT, &act, &oldact);

    printf("[%s,%d] Initializing IPCs...\n",__FUNCTION__,__LINE__);

    initializeInfo();

    printf("Starting sub processes...\n");
    startCollectingProcesses();

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

            case 'r' :
                stopAllCollectingProcesses(-1);
                if(deviceInfoPtr != NULL)
                    releaseResource();
                initializeInfo();
                startCollectingProcesses();
                break;

            case 'x' :
                stopAllCollectingProcesses(-1);
                if(deviceInfoPtr != NULL)
                    releaseResource();
                printf("bye\n");
                exit(0);
                break;
            case 'u' :
                break;

            default :
                printf("commands : \n");
                printf("l - list contents of the device conf and data conf\n");
                printf("r - Restart \n");
                printf("u - start uploading program \n");
                printf("x - close the program\n");
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

void startCollectingProcesses()
{

    pid_t pid = 0;
    char buf[128];
    deviceInfo *ptr = deviceInfoPtr;

    printf("[%s,%d] Starting sub processes...\n\n",__FUNCTION__,__LINE__);

    while(ptr != NULL)
    {

        //fork sub process

        if(1)
        {

            pid = fork();

            //decide what to do by pid

            if(pid == 0)
            {

                //opening sub process

                memset(buf,'\0',sizeof(buf));
                sprintf(buf,"./%s",ptr->deviceName);
                printf("starting %s with pid %d\n\n",buf,getpid());
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

        }

        //next deviceInfo

        ptr = ptr->next;

    }
    return ;

}

void stopAllCollectingProcesses(int sigNum)
{

    int ret = 0;
    pid_t pid = 0;
    char buf[128];
    deviceInfo *ptr = deviceInfoPtr;
    FILE *pidFile = NULL;

    printf("[%s,%d] Stopping sub processes...\n\n",__FUNCTION__,__LINE__);

    while(ptr != NULL)
    {

        sprintf(buf,"./pid/%s",ptr->deviceName);
        pidFile = fopen(buf,"r");

        if(ptr->subProcessPid > 0)
        {

            printf("[%s,%d] Stopping %s now (stored pid is %d) \n",__FUNCTION__,__LINE__,ptr->deviceName,ptr->subProcessPid);
            ret = kill(ptr->subProcessPid,SIGUSR1);
            if(ret < 0)
            {

                printf("[%s,%d] kill failed %s\n",__FUNCTION__, __LINE__, strerror(ret));
                
            }

        }
        else if(pidFile != NULL)
        {

            printf("[%s,%d] %s stored pid is %d \n",__FUNCTION__,__LINE__,buf,ptr->subProcessPid);
            ret = kill(ptr->subProcessPid,SIGUSR1);
            if(ret < 0)
            {

                printf("[%s,%d] kill failed %s\n",__FUNCTION__, __LINE__, strerror(ret));
                
            }

        }
        else
        {
            printf("[%s,%d] skipping %s , pid is %d \n",__FUNCTION__,__LINE__,ptr->deviceName,ptr->subProcessPid);
        }
        
        ptr = ptr->next;

        if(pidFile != NULL)
            fclose(pidFile);

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
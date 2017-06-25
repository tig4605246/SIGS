/*

    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 8,2017
    Program statement: 
        This program will manage all other sub processes it creates.
        Also, it's responsible for invoking and terminating all sub processes.

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"

#define CMDLEN 128
#define BUFLEN 2048

#define TMP_PATH "./tmp.conf"

//Store shared memory id

int shmID = 0;

dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;

//Intent : Set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre    : Nothing
//Post   : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();

//Intent : Free deviceInfoPtr, dataInforPtr and free the shared memory (get pointers from global parameters)
//Pre    : Nothing
//Post   : On success, return 0. On error, return -1 and shows the error messages

void releaseResource();

//Intent : Start sub processes
//Pre    : Nothing
//Post   : On success, return 0. On error, return -1 and shows the error messages

void startCollectingProcesses();

//Intent : Stop sub processes by using the pid stored in the deviceInfo struct
//Pre    : Nothing
//Post   : Nothing

void stopAllCollectingProcesses();

//Intent : Write data to shared memory
//Pre    : Nothing
//Post   : Nothing

void testWriteSharedMemory();

//Intent : Start GW-to-Server processes
//Pre    : Nothing
//Post   : Nothing

void stratUploadingProcesses();

//Intent : Stop GW-to-Server processes
//Pre    : Nothing
//Post   : Nothing

void stopUploadingProcesses();

//Intent : Shut down everything when SIGINT is catched
//Pre    : Nothing
//Post   : Nothing

//Intent : 

void forceQuit(int sigNum);

//Intent : This function will update the conf by cpm70-agent, if the process failed, it will call the backup conf file
//Pre    : Nothing
//Post   : Nothing

void updateConf();

/*

//Intent : Restart sub-processes
//Pre    : Nothing
//Post   : Nothing

void RestartSubProcesses();

*/


int main()
{

    int i, ret = 0;
    char input;
    FILE *daemonFp = NULL;
    struct sigaction act, oldact;

    struct sigaction act_2, oldact_2;

    struct sigaction act_3, oldact_3;
    
    
    printf("Starting SGSmaster...\n");

    showVersion();


    //sgsShowDeviceInfo(deviceInfoPtr);

    //catching SIGINT

    act.sa_handler = (__sighandler_t)forceQuit;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGINT, &act, &oldact);

    //catching SIGTERM

    act_2.sa_handler = (__sighandler_t)forceQuit;
    act_2.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act_2, &oldact_2);

    /*
    //catching SIGUSR2

    act_2.sa_handler = (__sighandler_t)RestartSubProcesses;
    act_2.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR2, &act_2, &oldact_2);
    */

    printf("[%s,%d] Updating data.conf...\n",__FUNCTION__,__LINE__);

    updateConf();

    printf("[%s,%d] Initializing IPCs...\n",__FUNCTION__,__LINE__);

    initializeInfo();

    printf("Starting sub processes...\n");

    startCollectingProcesses();

    while(1)
    {
        /*
        printf("$-> ");
        scanf(" %c",&input);

        switch(input)
        {

            case 'l' :

                sgsShowAll(deviceInfoPtr);
                break;

            case 'r' :

                stopAllCollectingProcesses();
                if(deviceInfoPtr != NULL)
                    releaseResource();
                initializeInfo();
                startCollectingProcesses();
                break;

            case 'x' :

                stopAllCollectingProcesses();
                if(deviceInfoPtr != NULL)
                    releaseResource();
                printf("bye\n");
                exit(0);
                break;

            case 'u' :

                break;

            case 't' :

                testWriteSharedMemory();
                
                break;

            default :

                // printf("commands : \n");
                // printf("l - list contents of the device conf and data conf\n");
                // printf("r - Restart \n");
                // printf("u - start uploading program \n");
                // printf("x - close the program\n");
                // printf("\n");
                break;

        }
        */
        usleep(100000);

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

    ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 1);
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

    sgsDeleteAll(deviceInfoPtr,shmID);

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

void stopAllCollectingProcesses()
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

void forceQuit(int sigNum)
{

    stopAllCollectingProcesses();

    if(deviceInfoPtr != NULL)
        releaseResource();
        
    printf("Signal Catched (signal number %d), SGSmaster is forceQuitting...\n",sigNum);

    exit(0);

}

void testWriteSharedMemory()
{

    dataLog source;
    dataInfo *ptr = deviceInfoPtr->dataInfoPtr;
    int i = 0;


    while(ptr != NULL)
    {

        source.valueType = INTEGER_VALUE;
        source.value.i = i;
        sgsWriteSharedMemory(ptr, &source);
        ptr = ptr->next;
        i++;

    }
    

    return ;

}

void updateConf()
{

    FILE *fp = NULL;
    FILE *fpRead = NULL;
    char cmd[CMDLEN];
    char buf[BUFLEN];
    char *returnValue = NULL;
    char agent_name[] = "cpm70-agent-tx";
    char grid_id[32];
    int grid_number = 0, i = 0;
    int ret = 0;

    fp = fopen(TMP_PATH,"w");

    if(fp == NULL)
    {

        printf(LIGHT_RED"[%s,%d] Error, can't open %s. This is probably caused by permission or incorrect path\n"NONE,__FUNCTION__,__LINE__,DATACONF);
        return ;

    }

    fprintf(fp,
        "#data info #deviceName, SensorName, valueName, ID, read address, read length, (optional)\n"
        "GWInfo,System_Info,CPU_Usage,01,02,03,\n"
        "GWInfo,System_Info,Memory_Usage,01,02,03,\n"
        "GWInfo,System_Info,Disk_Usage,01,02,03,\n"
    );

    
    //Ready the command at here

    memset(cmd,0,sizeof(sizeof(cmd)));

    snprintf(cmd,CMDLEN,"/home/aaeon/API/cpm70-agent-tx --list-all");

    printf("%s\n",cmd);

    //execute command

    fpRead = popen(cmd,"r");

    //Read result to buf

    if(fpRead != NULL)
        fgets(buf, BUFLEN , fpRead);
    else
    {

        printf("[%s,%d]execute command : %s failed\n",__FUNCTION__,__LINE__,cmd);
        fclose(fp);
        return;

    }

    printf("buf is %s",buf);

    //if ok, we update the data.conf

    returnValue = strstr(buf,"ok");

    if(returnValue == NULL)
    {

        printf(LIGHT_RED"[%s,%d]Reply of the command : %s\nFailed to get power grids' IDs\n"NONE,__FUNCTION__,__LINE__,cmd);
        fclose(fp);
        fclose(fpRead);
        return;

    }

    //Get how many power grids we have at this GW

    returnValue = strstr(buf,";");
    *returnValue = 0;
    returnValue++;

    grid_number = atoi(returnValue);

    printf("[%s,%d]grid_num %d\n",__FUNCTION__,__LINE__,grid_number);

    for(i = 0 ; i < grid_number ; i++)
    {

        //initialize char array

        memset(grid_id,0,sizeof(grid_id));
        memset(buf,0,sizeof(buf));
        
        //Get grid id
        
        fgets(buf, BUFLEN , fpRead);
        snprintf(grid_id,32,"%s",buf);
        grid_id[strlen(grid_id) - 1] = 0;

        fprintf(fp,
           "cpm70_agent,%s,ID,01,02,03,\n"
            "cpm70_agent,%s,lastReportTime,01,02,03,\n"
            "cpm70_agent,%s,wire,01,02,03,\n"
            "cpm70_agent,%s,freq,01,02,03,\n"
            "cpm70_agent,%s,ua,01,02,03,\n"
            "cpm70_agent,%s,ub,01,02,03,\n"
            "cpm70_agent,%s,uc,01,02,03,\n"
            "cpm70_agent,%s,u_avg,01,02,03,\n"
            "cpm70_agent,%s,uab,01,02,03,\n"
            "cpm70_agent,%s,ubc,01,02,03,\n"
            "cpm70_agent,%s,uca,01,02,03,\n"
            "cpm70_agent,%s,uln_avg,01,02,03,\n"
            "cpm70_agent,%s,ia,01,02,03,\n"
            "cpm70_agent,%s,ib,01,02,03,\n"
            "cpm70_agent,%s,ic,01,02,03,\n"
            "cpm70_agent,%s,i_avg,01,02,03,\n"
            "cpm70_agent,%s,pa,01,02,03,\n"
            "cpm70_agent,%s,pb,01,02,03,\n"
            "cpm70_agent,%s,pc,01,02,03,\n"
            "cpm70_agent,%s,p_sum,01,02,03,\n"
            "cpm70_agent,%s,sa,01,02,03,\n"
            "cpm70_agent,%s,sb,01,02,03,\n"
            "cpm70_agent,%s,sc,01,02,03,\n"
            "cpm70_agent,%s,s_sum,01,02,03,\n"
            "cpm70_agent,%s,pfa,01,02,03,\n"
            "cpm70_agent,%s,pfb,01,02,03,\n"
            "cpm70_agent,%s,pfc,01,02,03,\n"
            "cpm70_agent,%s,pf_avg,01,02,03,\n"
            "cpm70_agent,%s,ae_tot,01,02,03,\n"
            "cpm70_agent,%s,uavg_thd,01,02,03,\n"
            "cpm70_agent,%s,iavg_thd,01,02,03,\n"
            ,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id
            ,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id
            ,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id
            ,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id,grid_id
            ,grid_id,grid_id,grid_id
        );

    }

    fclose(fp);
    fclose(fpRead);

    //replace the old conf file

    if(ret = rename(TMP_PATH,DATACONF) != 0)
    {

        printf("[%s,%d]rename %s failed, we are using the old conf file\n",__FUNCTION__,__LINE__,TMP_PATH);

    }


    return;

}
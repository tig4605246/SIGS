/*
    Name: Xu Xi-Ping
    Date: April 21,2017
    Last Update: April 21,2017
    Program statement: 
        This program will open db and record the logs

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>


//We declare our own libraries at below

#include "../controlling/SGScontrol.h"
#include "../log/SGSlogfile.h"

#define DBNAME "logtest.db"

dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;

//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();

//Intent : shut down everything when SIGINT is catched
//Pre : Nothing
//Post : Nothing

void forceQuit(int sigNum);


int main()
{

    struct sigaction act, oldact;
    int ret = 0;
    sqlite3 *db = NULL;


    ret = sgsInitControl("SGSlogger");
    if(ret < 0)
    {

        printf("SGSlogger aborting\n");
        return -1;

    }
    
    printf("Starting SGSlogger...\n");

    act.sa_handler = (__sighandler_t)forceQuit;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR2, &act, &oldact);

    printf("[%s,%d] Initializing IPCs...\n",__FUNCTION__,__LINE__);

    initializeInfo();

    if(deviceInfoPtr == NULL)
    {
        printf(LIGHT_RED"[%s,%d] deviceInfoPtr is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;
    }
    else
    {

        while(strcmp(deviceInfoPtr->deviceName,"GWInfo") && (deviceInfoPtr != NULL))
            deviceInfoPtr = deviceInfoPtr->next;
    }

    //test our logfile functions at here

    ret = sgsOpenSqlDB(DBNAME , &db);
    if(ret != 0)
    {
        printf(LIGHT_RED"[%s,%d] sgsOpenSqlDB failed\n"NONE,__FUNCTION__,__LINE__);
        return -1;
    }

    ret = sgsCreateTable(db, deviceInfoPtr);
    if(ret != 0)
    {
        printf(LIGHT_RED"[%s,%d] sgsCreateTable failed\n"NONE,__FUNCTION__,__LINE__);
        return -1;
    }

    //Delay some time before recording
    while(1)
    {
        sleep(10);

        ret = sgsNewRecord(db, deviceInfoPtr);
        if(ret != 0)
        {
            printf(LIGHT_RED"[%s,%d] sgsNewRecord failed\n"NONE,__FUNCTION__,__LINE__);
            return -1;
        }
    }
    ret = sgsRetreiveRecordsByTime(db, deviceInfoPtr, time(NULL));
    if(ret != 0)
    {
        printf(LIGHT_RED"[%s,%d] sgsNewRecord failed\n"NONE,__FUNCTION__,__LINE__);
        return -1;
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

    ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 0);
    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    return 0;

}

void forceQuit(int sigNum)
{

    
    printf("[SIGUSR2] Catched (signal number %d) , forceQuitting...\n",sigNum);
    sgsDeleteAll(deviceInfoPtr,-1);
    exit(0);

}
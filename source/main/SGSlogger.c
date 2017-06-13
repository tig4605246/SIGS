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
#include <time.h>

#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>


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

//Intent : a sample callback function
//Pre : parameters provided from sqlite3
//Post : On success, return 0. On error, return -1

static int sampleCallback(void *NotUsed, int argc, char **argv, char **azColName);


int main()
{

    struct sigaction act, oldact;
    int ret = 0;
    sqlite3 *db = NULL;
    deviceInfo *deviceTemp = NULL;
    epochTime now;


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

    deviceTemp = deviceInfoPtr;

    //test our logfile functions at here

    ret = sgsOpenSqlDB(DBNAME , &db);
    if(ret != 0)
    {
        printf(LIGHT_RED"[%s,%d] sgsOpenSqlDB failed\n"NONE,__FUNCTION__,__LINE__);
        return -1;
    }

    while(deviceTemp != NULL)
    {

        if(strcmp("SGSlogger",deviceTemp->deviceName))
        {

            printf("deviceName : %s\n",deviceTemp->deviceName);
            ret = sgsCreateTable(db, deviceTemp);

            if(ret != 0)
            {

                printf(LIGHT_RED"[%s,%d] sgsCreateTable for %s failed\n"NONE,__FUNCTION__,__LINE__,deviceTemp->deviceName);

            }

        }
        deviceTemp = deviceTemp->next;
         
    }

    
    //Delay some time before recording

    while(1)
    {

        sleep(30);

        //Loop every device and store their data

        while(deviceTemp != NULL)
        {

            //Ignore itself

            if(strcmp("SGSlogger",deviceTemp->deviceName))
            {

                printf(YELLOW"Newing records\n"NONE);
                ret = sgsNewRecord(db, deviceTemp, NULL);

                if(ret != 0)
                {

                    printf(LIGHT_RED"[%s,%d] sgsNewRecord failed\n"NONE,__FUNCTION__,__LINE__);
                
                }

            } 

            //Check if we got some outdated data logs need to be deleted (now - 86400*7 )

            sgsDeleteRecordsByTime(db, deviceTemp, now - 60);

            deviceTemp = deviceTemp->next;

        }

        time(&now);


        

        deviceTemp = deviceInfoPtr;
        
    }

    /*
    ret = sgsRetrieveRecordsByTime(db, deviceInfoPtr, time(NULL), sampleCallback);
    printf(LIGHT_RED"[%s,%d] time(NULL) is %ld\n"NONE,__FUNCTION__,__LINE__,time(NULL));
    

    if(ret != 0)
    {
        printf(LIGHT_RED"[%s,%d] sgsRetrieveRecordsByTime failed\n"NONE,__FUNCTION__,__LINE__);
        return -1;
    }
    */

    sgsDeleteAll(deviceInfoPtr,0);
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

static int sampleCallback(void *NotUsed, int argc, char **argv, char **azColName)
{

    int i;
   for(i=0; i<argc; i++)
   {

      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");

   }

   printf("\n");
   printf(LIGHT_PURPLE"[%s,%d] this is a callback function from SGSlogger\n"NONE,__FUNCTION__,__LINE__);

   return 0;

}
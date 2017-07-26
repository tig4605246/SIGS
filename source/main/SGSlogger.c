/*

    Name: Xu Xi-Ping
    Date: April 21,2017
    Last Update: July 26,2017
    Program statement: 
        This program currently is built for Solar project.
        
        Main functions:
        1. Create logs
        2. Delete outdated logs

        That's it.

        Every activated data buffer will be used to form the log datatable.

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
#include "../events/SGSEvent.h"

#define DBNAME "logtest.db"
#define AUTOLIST "./conf/Collect/AutoList"

//Intent    : shut down everything when SIGINT is catched
//Pre       : Nothing
//Post      : Nothing

void forceQuit(int sigNum);

//Intent    : a sample callback function
//Pre       : parameters provided from sqlite3
//Post      : On success, return 0. On error, return -1

static int sampleCallback(void *NotUsed, int argc, char **argv, char **azColName);

//Intent    : Create new log in sqlite3 db
//Pre       : Nothing
//Post      : On success, return 0. Otherwise return -1

int CreateTable();

//Intent    : Save buffer status as a sqlite3 data
//Pre       : Nothing
//Post      : On success, return 0. Otherwise return -1

int SaveLog();

int CheckAndRespondQueueMessage();

//Intent    : Delete outdated log
//Pre       : Nothing
//Post      : On success, return 0. Otherwise return -1

int DeleteLog();

int logId = -1;                 // logger message queue id
int eventHandlerId = -1;        // Event-Handler queue id
int interval = 5;              // time between each collecting
dataInfo *dInfo[MAXBUFFERINFOBLOCK];             // 0 for inverter, 1 for irr & temp, 2 for GWInfo
DataBufferInfo bufferInfo[MAXBUFFERINFOBLOCK];   // dataBufferInfo from buffer pool
sqlite3 *db = NULL;             // db file pointer
int dataSize = 0;               // accumulate the numberOfData for malloc()

#if 1

int main(int argc, char *argv[])
{

    int i = 0, ret = 0;
    char buf[512];
    char *name = NULL;
    char *path = NULL;
    FILE *fp = NULL;
    time_t last, now;
    struct sigaction act, oldact;
     
    printf("SGSlogger starts---\n");

    memset(buf,'\0',sizeof(buf));

    act.sa_handler = (__sighandler_t)forceQuit;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    //Ignore SIGINT
    
    signal(SIGINT, SIG_IGN);

    //Open signal queue

    eventHandlerId = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);
    if(eventHandlerId == -1)
    {
        printf("Open eventHandler queue failed...\n");
        exit(0);
    }

    sgsSendQueueMsg(eventHandlerId, "[BOOT];Logger", EnumLogger);

    logId = sgsCreateMsgQueue(LOGGER_KEY, 0);
    if(logId == -1)
    {
        printf("Open Logger queue failed...\n");
        exit(0);
    }

    //Init dInfo and bufferInfo

    for(i = 0 ; i < MAXBUFFERINFOBLOCK ; i++)
    {

        memset(&(bufferInfo[i]),0,sizeof(DataBufferInfo));
        dInfo[i] = NULL;

    }


    //Attach buffer pool

    ret = sgsInitBufferPool(0);
    if(ret == -1)
    {

        snprintf(buf,sizeof(buf),"%s;Failed to init buffer pool, return code %d", ERROR, ret);
        ret = sgsSendQueueMsg(eventHandlerId, buf, EnumLogger);
        exit(0);

    }

    sleep(10);

    /*
     *
     * Attach the dataInfo
     * If we want to make this dynamic, just read the names from the AutoList of the collector
     *
     */


    //Read in list of auto start agent

    fp = fopen(AUTOLIST, "r");
    if(fp == NULL)
    {

        snprintf(buf,sizeof(buf),"%s;No auto start list, shutting down",ERROR);
        sgsSendQueueMsg(eventHandlerId, buf, EnumLogger);
        exit(0);

    }

    //

    i = 0;

    while(i < MAXBUFFERINFOBLOCK)
    {

        memset(buf, '\0', sizeof(buf));

        //Read a line from the AutoList

        fgets(buf, 128, fp);

        if(buf[strlen(buf) - 1] == '\n')    buf[strlen(buf) - 1] = '\0';

        printf("buf is [%s]\n",buf);

        //If the buf is "#END" leave this section

        if(!strcmp("#END", buf))    break;

        //If the buf is started with "#", we should skip it

        if(buf[0] == '#')   continue;

        //Prepare the info

        name = strtok(buf,";");
        path = strtok(NULL,";");

        printf("path to %s is %s\n",name,path);

        //Get data info from pool

        ret = sgsGetDataInfoFromBufferPool(name, &(bufferInfo[i]));
        if(ret == -1)
        {

            printf("Failed to get data buffer info, return %d\n", ret);
            exit(0);

        }

        //calculate the number of the data

        dataSize += bufferInfo[i].numberOfData;

        //Attach the shared memory

        memset(buf,'\0',sizeof(buf));
        snprintf(buf, 511, "./conf/Collect/%s",bufferInfo[i].dataName);
        ret = sgsInitDataInfo(NULL, &(dInfo[i]), 0, buf, bufferInfo[i].shmId, NULL);
        if(ret == -1)
        {

            snprintf(buf, sizeof(buf) - 1, "%s;Attach shmid %d Failed (config path %s)\n", ERROR, bufferInfo[i].shmId, buf);
            sgsSendQueueMsg(eventHandlerId, buf, EnumLogger);
            exit(0);

        }
        i++;

    }

    fclose(fp);

    ret = sgsOpenSqlDB(DBNAME , &db);
    if(ret != 0)
    {

        printf(LIGHT_RED"[%s,%d] sgsOpenSqlDB failed\n"NONE,__FUNCTION__,__LINE__);
        forceQuit(15);

    }

    //Create db Table

    ret = CreateTable();
    if(ret != 0)
    {

        printf(LIGHT_RED"[%s,%d] Create Table failed\n"NONE,__FUNCTION__,__LINE__);
        forceQuit(15);

    }

    //get first timestamp

    time(&last);
    now = last;

    //main loop

    while(1) 
    {

        usleep(100000);
        time(&now);

        //check time interval

        if( (now-last) >= interval )
        {

            //Update data
            //printf("generate new data\n");
            ret = SaveLog();
            //printf("show data\n");
            //sgsShowDataInfo(dInfo);
            //printf("got new time\n");
            time(&last);
            last += 4;
            now = last;

        }

        //Check message

        ret = CheckAndRespondQueueMessage();

    }

}


#else
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
#endif

void forceQuit(int sigNum)
{
    
    printf("Signal catched (signal number %d) , forceQuitting...\n",sigNum);
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

int CreateTable()
{

    char buf[DATAVALUEMAX];
    char sensorName[DATAVALUEMAX];
    char *table = NULL;
    char *zErrMsg = NULL;
    int i = 0, ret = -1;
    dataInfo *temp;

    //Construct the sql command buffer

    table = (char *)malloc(sizeof(char ) * DATAVALUEMAX * (dataSize));

    //Clear the char array

    memset(table,'\0',sizeof(table));
    memset(buf,'\0',sizeof(buf));

    //Start to fill the command for creating table

    snprintf(buf,DATAVALUEMAX,"CREATE TABLE SOLAR(");

    strcat(table,buf);

    snprintf(buf,DATAVALUEMAX,"Timestamp NUMERIC         NOT NULL,");

    strcat(table,buf);

    snprintf(buf,DATAVALUEMAX,"sensorName    CHAR(%d)    NOT NULL,",DATAVALUEMAX);

    strcat(table,buf);

    i = 0;

    while(i < MAXBUFFERINFOBLOCK)
    {

        temp = dInfo[i];

        if(temp == NULL) break;

        while(temp != NULL)
        {

            snprintf(buf,DATAVALUEMAX,"%s    CHAR(%d)    NOT NULL,",temp->valueName,DATAVALUEMAX);
            strcat(table,buf);
            temp = temp->next;

        }
       
        i++;

    }

    snprintf(buf,DATAVALUEMAX,"PRIMARY KEY (Timestamp, sensorName));");

    strcat(table,buf);

    printf("table is : \n%s\n\n",table);

    ret = sqlite3_exec(db, table, NULL, 0, &zErrMsg);
    if( ret != SQLITE_OK )
    {

        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        snprintf(buf,sizeof(buf),"%s;%s, Logger creates table failed",ERROR, zErrMsg);
        sgsSendQueueMsg(eventHandlerId, buf, EnumLogger);
        sqlite3_free(zErrMsg);

    }else
    {

        fprintf(stdout, "Table created successfully\n");

    }

    free(table);
    return 0;

}

int SaveLog()
{

    char buf[DATAVALUEMAX];
    char sensorName[DATAVALUEMAX];
    char insertBuf[DATAVALUEMAX];
    char *table = NULL;
    char *zErrMsg = NULL;
    int i = 0, ret = -1;
    dataInfo *temp;
    dataLog dLog;

    //Prepare the INSERT datatable
    
    table = malloc(sizeof(char ) * DATAVALUEMAX * dataSize);
    memset(table, 0, sizeof(sizeof(char ) * DATAVALUEMAX * dataSize));

    memset(buf,0,sizeof(buf));

    memset(insertBuf,0,sizeof(insertBuf));

    snprintf(buf,256,"INSERT INTO SOLAR (");
    strcat(insertBuf,buf);

    memset(buf,0,sizeof(buf));

    snprintf(buf,256,"Timestamp,");

    strcat(insertBuf,buf);

    memset(buf,0,sizeof(buf));

    snprintf(buf,256,"sensorName,");

    strcat(insertBuf,buf);

    i = 0;

    while(i < MAXBUFFERINFOBLOCK)
    {

        //Create table header

        if(dInfo[i] != NULL)
        {

            temp = dInfo[i];

            while(temp != NULL)
            {

                memset(buf,0,sizeof(buf));
                snprintf(buf,256,"%s,",temp->valueName);
                strcat(insertBuf,buf);
                temp = temp->next;

            }

        }
        else
        {
            break;
        }

        i++;

    }

    if(strlen(insertBuf) > 0) insertBuf[strlen(insertBuf) - 1] = 0;

    //Finished the INSERT table shape

    memset(buf,0,sizeof(buf));

    snprintf(buf,256,")");

    strcat(insertBuf,buf);

    //Traverse through every data buffer and collect the data

    //Get record time

    memset(buf,0,sizeof(buf));

    snprintf(buf,256,"VALUES (strftime('%%s','now','localtime'),");

    strcat(insertBuf,buf);

    //Get sensorName

    memset(buf,0,sizeof(buf));

    snprintf(buf,256," 'Solar',");

    strcat(insertBuf,buf);

    strcat(table, insertBuf);

    i = 0;

    while(i < MAXBUFFERINFOBLOCK)
    {

        //Check Availability

        if(dInfo[i] != NULL)
        {

            temp = dInfo[i];
            while(temp != NULL)
            {

                sgsReadSharedMemory(temp, &dLog);
                memset(buf,0,sizeof(buf));
                switch(dLog.valueType)
                {

                    case INTEGER_VALUE:
                        snprintf(buf,256," '%d',",dLog.value.i);
                        strcat(table,buf);
                    break;

                    case STRING_VALUE:
                        snprintf(buf,256," '%s',",dLog.value.s);
                        strcat(table,buf);
                    break;
                    
                    case FLOAT_VALUE:
                        snprintf(buf,256," '%s',",dLog.value.s);
                        strcat(table,buf);
                    break;

                    default:
                        snprintf(buf,256,"%s;Logger [%s,%d] UNKNOWN VALUE Occured in %s, valueType %d", ERROR, __FUNCTION__, __LINE__, temp->sensorName, dLog.valueType);
                        sgsSendQueueMsg(eventHandlerId,buf,EnumLogger);
                        free(table);
                        return -1;
                    break;

                }
                
                temp = temp->next;

            }

        }
        else
        {

            snprintf(buf,256,"%s;Logger [%s,%d] dInfo %d is empty", ERROR, __FUNCTION__, __LINE__, i);
            break;
            
        }
        i++;

    }

    //Complete the command with );

    memset(buf, 0, sizeof(buf));
    snprintf(buf, 256, ");");
    if(strlen(table) > 0) table[strlen(table) - 1] = 0;
    strcat(table, buf);

    printf("New data, table is:\n%s\n", table);

    ret = sqlite3_exec(db, table, NULL, 0, &zErrMsg);
    if( ret != SQLITE_OK )
    {

        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        snprintf(buf,256,"%s;Logger [%s,%d] SQL error: %s", ERROR, __FUNCTION__, __LINE__, zErrMsg);
        sgsSendQueueMsg(eventHandlerId, buf, EnumLogger);
        sqlite3_free(zErrMsg);

    }else
    {

        fprintf(stdout, "New record successfully\n");

    }

    free(table);

    return 0;


}

int CheckAndRespondQueueMessage()
{

    int ret = -1;
    char buf[MSGBUFFSIZE];

    memset(buf,0,sizeof(buf));

    ret = sgsRecvQueueMsg(logId, buf, 0);

    if(ret != -1)
    {

        printf("SGSlogger Get message:\n%s\n",buf);

    }

    return 0;

}
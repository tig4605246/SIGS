/*

    Name: Xi-Ping Xu
    Date: July 19,2017
    Last Update: July 19,2017
    Program statement: 
        This is a agent used to test SGS system. 
        It has following functions:
        1. Get a data buffer info from the data buffer pool
        2. Show fake data.
        3. Issue Command to FakeTaida and receive result

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"

int GenerateAndUpdateData();

int CheckAndRespondQueueMessage();

int ShutdownSystemBySignal(int sigNum);

dataInfo *dInfo;    // pointer to the shared memory
int interval = 10;  // time period between last and next collecting section
int eventHandlerId; // Message queue id for event-handler
int shmId;          // shared memory id
int msgId;          // created by sgsCreateMessageQueue
int msgType;        // 0 1 2 3 4 5, one of them

int main(int argc, char *argv[])
{

    int i = 0, ret = 0, numberOfData = 0;
    char buf[512];
    FILE *fp = NULL;
    time_t last, now;
    struct sigaction act, oldact;
    DataBufferInfo bufferInfo;  

    printf("SolarPost starts---\n");

    memset(buf,'\0',sizeof(buf));

    snprintf(buf,511,"%s;argc is %d, argv 1 %s", LOG, argc, argv[1]);

    msgType = atoi(argv[1]);

    act.sa_handler = (__sighandler_t)ShutdownSystemBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    //Ignore SIGINT
    
    signal(SIGINT, SIG_IGN);

    eventHandlerId = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);
    if(eventHandlerId == -1)
    {
        printf("Open eventHandler queue failed...\n");
        exit(0);
    }

    msgId = sgsCreateMsgQueue(UPLOAD_AGENT_KEY, 0);
    if(msgId == -1)
    {
        printf("Open Upload Agent queue failed...\n");
        exit(0);
    }

    dInfo = NULL;

    //Attach buffer pool

    ret = sgsInitBufferPool(0);

    //Get data info

    ret = sgsGetDataInfoFromBufferPool("FakeTaida", &bufferInfo);
    if(ret == -1)
    {

        printf("Failed to get data buffer info, return %d\n", ret);
        exit(0);

    }

    printf("Number of data is %d \n",bufferInfo.numberOfData);

    //Attach the dataInfo

    ret = sgsInitDataInfo(NULL, &dInfo, 0, "./conf/Collect/FakeTaida", bufferInfo.shmId, &numberOfData);
    if(ret == -1)
    {

        printf("Attach shmid %d Failed\n",bufferInfo.shmId);
        exit(0);

    }

    //update data

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
            ret = GenerateAndUpdateData();
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

int GenerateAndUpdateData()
{

    dataInfo *tmpInfo = NULL;
    dataLog dLog;
    char buf[128];

    tmpInfo = dInfo ;
    while(tmpInfo != NULL)
    {
        
        sgsReadSharedMemory(tmpInfo, &dLog);

        memset(buf,'\0',sizeof(buf));

        strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &(dLog.updatedTime));

        if(!strcmp(tmpInfo->valueName,"AC1"))
        {

            printf("AC1: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"AC2"))
        {

            printf("AC2: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"AC3"))
        {

            printf("AC3: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"DC1"))
        {

            printf("DC1: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"DC2"))
        {

            printf("DC2: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"Daily"))
        {

            printf("Daily: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"Error"))
        {

            printf("Error: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        
        tmpInfo = tmpInfo->next;

    }
    return 0;

}

int CheckAndRespondQueueMessage()
{

    int ret = -1;
    char buf[MSGBUFFSIZE];
    char *cmdType = NULL;
    char *from = NULL;
    char *to = NULL;
    char *content = NULL;
    char result[MSGBUFFSIZE];

    memset(buf,'\0',sizeof(buf));

    ret = sgsRecvQueueMsg(msgId, buf, msgType);

    if(ret != -1)
    {

        cmdType = strtok(buf,SPLITTER);
        if(cmdType == NULL)
        {
            printf("Can't get the command type. the message is incomplete\n");
            return -1;
        }

        to = strtok(NULL,SPLITTER);
        if(to == NULL)
        {
            printf("Can't get the to. the message is incomplete\n");
            return -1;
        }

        from = strtok(NULL,SPLITTER);
        if(from == NULL)
        {
            printf("Can't get the from. the message is incomplete\n");
            return -1;
        }

        //return result

        memset(result,'\0',sizeof(result));

        snprintf(result,MSGBUFFSIZE - 1,"%s;%s;%s;command done.",RESULT,from,to);

        sgsSendQueueMsg(eventHandlerId, result, msgId);

        return 0;
    
    }
    return 0;

}

int ShutdownSystemBySignal(int sigNum)
{

    printf("SolarPost bye bye\n");
    sgsDeleteDataInfo(dInfo, -1);
    exit(0);

}
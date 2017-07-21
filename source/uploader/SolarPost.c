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

int RegisterDataBuffer(int sharedMemoryId, int numberOfInfo);

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

    int i = 0, ret = 0;
    char buf[512];
    FILE *fp = NULL;
    time_t last, now;
    struct sigaction act, oldact;  

    printf("FakeTaida starts---\n");

    memset(buf,'\0',sizeof(buf));

    snprintf(buf,511,"%s;argc is %d, argv 1 %s", LOG, argc, argv[1]);

    sgsSendQueueMsg(eventHandlerId,buf,9);

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

    dInfo = NULL;
    shmId = -1;



    ret = sgsInitDataInfo(NULL, &dInfo, 1, "./conf/Collect/FakeTaida");

    if(ret < 0 )
    {

        printf("failed to create dataInfo, ret is %d\n",ret);
        sgsSendQueueMsg(eventHandlerId,"[Error];failed to create dataInfo",9);
        exit(0);

    }

    printf("ret return %d\n", ret);

    //Store shared memory id

    shmId = ret;

    //Show data

    //printf("Show dataInfo\n");
    //sgsShowDataInfo(dInfo);

    //Attach buffer pool

    ret = sgsInitBufferPool(0);

    //Registration

    ret = sgsRegisterDataInfoToBufferPool("FakeTaida", shmId, 7);
    if(ret == -1)
    {

        printf("Failed to register, return %d\n", ret);
        sgsDeleteDataInfo(dInfo, shmId);
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

        if(((now%interval) == 0) || ((now-last) >= interval ))
        {

            //Update data
            //printf("generate new data\n");
            ret = GenerateAndUpdateData();
            //printf("show data\n");
            //sgsShowDataInfo(dInfo);
            //printf("got new time\n");
            sleep(1);
            time(&last);
            now = last;

        }

        //Check message

        ret = CheckAndRespondQueueMessage();

    }

}

int RegisterDataBuffer(int sharedMemoryId, int numberOfInfo)
{

    int ret = -1;

    ret = sgsRegisterDataInfoToBufferPool("FakeTaida", sharedMemoryId, numberOfInfo);
    if(ret != 0)
    {

        printf("Registration failed, return %d\n", ret);
        printf("Delete dataInfo, shmid %d\n",ret);
        sgsDeleteDataInfo(dInfo, ret);
        exit(0);

    }
    return 0;

}

int GenerateAndUpdateData()
{

    dataInfo *tmpInfo = NULL;
    dataLog dLog;

    tmpInfo = dInfo ;
    while(tmpInfo != NULL)
    {

        if(!strcmp(tmpInfo->valueName,"AC1"))
        {

            dLog.value.i = rand()%100 + 3000;

        }
        else if(!strcmp(tmpInfo->valueName,"AC2"))
        {

            dLog.value.i = rand()%100 + 4000;

        }
        else if(!strcmp(tmpInfo->valueName,"AC3"))
        {

            dLog.value.i = rand()%100 + 5000;

        }
        else if(!strcmp(tmpInfo->valueName,"DC1"))
        {

            dLog.value.i = rand()%100 + 5000;

        }
        else if(!strcmp(tmpInfo->valueName,"DC2"))
        {

            dLog.value.i = rand()%100 + 5000;

        }
        else if(!strcmp(tmpInfo->valueName,"Daily"))
        {

            dLog.value.i = rand()%100 + 10000;

        }
        else if(!strcmp(tmpInfo->valueName,"Error"))
        {

            dLog.value.i = 0 + rand()%5;

        }

        dLog.valueType = INTEGER_VALUE;
        dLog.status = 1;

        sgsWriteSharedMemory(tmpInfo, &dLog);
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

    printf("FakeTaida bye bye\n");
    sgsDeleteDataInfo(dInfo, shmId);
    exit(0);

}
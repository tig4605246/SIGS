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
#include <sys/stat.h>


//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"

#define CMDLEN 128
#define BUFLEN 2048

void ShutdownSystemBySignal(int sigNum);

//Intent    :   record the log to log file
//Pre       :   path to log file, content of log
//Post      :   Nothing

int AddToLogFile(char *filePath, char *log);

//Intent    :   If the size of the log file surpass the limit, clear it.
//Pre       :   path to log file
//Post      :   Nothing

int CheckLogFileSize(char *filePath);

int SendMailToMaintainer(int msgType, char *content);

int main()
{

    int eventPid = 0;//own pid
    int id = -1;
    int ret;
    int sgsMasterId = -1; //msgid for queue message to master
    char buf[MSGBUFFSIZE];
    char *dataType = NULL;
    struct sigaction act, oldact;

    printf("Child: SGSeventhandler up\n");

    id = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);

    act.sa_handler = (__sighandler_t)ShutdownSystemBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    sgsMasterId = sgsCreateMsgQueue(SGSKEY, 0);
    if(sgsMasterId == -1)
    {
        printf("Open master queue failed...\n");
        exit(0);
    }

    while(1)
    {


        //Prepare to receive message

        ret = sgsRecvQueueMsg(id,buf,1);

        dataType = strtok(buf,SPLITTER);

        //  First, observe the content of the message. 
        //  There are three types of data: LOG | ERROR | CONTROL
        //  Log:        Store to the log file
        //  Error:      Email to the maintainer immediately
        //  Control:    Send message to the master
        if(ret != -1)
        {

            if(!strcmp(dataType,LOG))
            {

                CheckLogFileSize("./SGSlog");
                AddToLogFile("./SGSlog",buf);

            }
            else if(!strcmp(dataType,ERROR))
            {

                SendMailToMaintainer(ret, buf);

            }

            else if(!strcmp(dataType,CONTROL))
            {

                printf("SGSEventhandler got message: %s\n",buf);

                sgsSendQueueMsg(sgsMasterId, buf, ret);
                
            }

        }
            
        usleep(50000);

    }

    return 0;

}

void ShutdownSystemBySignal(int sigNum)
{

    printf("Handler bye bye\n");
    exit(0);

}

int AddToLogFile(char *filePath, char *log)
{

    FILE *fp = NULL;
    time_t t = time(NULL);
    DATETIME timeStruct  = *localtime(&t);

    fp = fopen(filePath,"a");

    if(fp == NULL)
    {

        printf("Failed to open the log file %s \n",filePath);
        return -1;

    }

    fprintf(fp,"[%d-%d-%d %d:%d:%d]%s\n"
        , timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday, timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec, log);

    fclose(fp);
    return 0;

}

int CheckLogFileSize(char *filePath)
{

    struct stat st;
    FILE *fp = NULL;
    int ret = -1;
    
    ret = stat(filePath, &st);

    if(ret == -1)
    {

        printf("No log file detected\n");
        return -1;

    }

    if(st.st_size > MAXLOGSIZE)
    {

        fp = fopen(filePath,"w");
        fclose(fp);

    }
    return 0;

}

int SendMailToMaintainer(int msgType, char *content)
{

    char message[1500];

    memset(message,'\0',sizeof(message));

    switch(msgType)
    {

        case EnumEventHandler:
        snprintf(message,1499,"EventHandler,%s", content);
        break;

        case EnumDataBuffer:
        snprintf(message,1499,"EventHandler,%s", content);
        break;

        case EnumCollector:
        snprintf(message,1499,"EventHandler,%s", content);
        break;

        case EnumUploader:
        snprintf(message,1499,"EventHandler,%s", content);
        break;

        case EnumLogger:
        snprintf(message,1499,"EventHandler,%s", content);
        break;

        default:
        printf(LIGHT_RED"Unknown msgtype %d\ncontent:%s\n"NONE,msgType,content);
        snprintf(message,1499,"EventHandler,%s", content);
        break;

    }

    sgsSendEmail(message);
    return 0;

}
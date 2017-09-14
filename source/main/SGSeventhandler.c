/*

    Name: Xi-Ping Xu
    Date: July 5,2017
    Last Update: July 21,2017
    Program statement: 
        It's a relay of all messages.
        In addition, it's also responsible for mailing agent

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
#include <sys/stat.h>


//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"

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

int mailAgentPid;

int mailAgentMsgId;

int main()
{

    int eventPid = 0;//own pid
    int id = -1;
    int ret;
    int sgsMasterId = -1; //msgid for queue message to master
    char buf[MSGBUFFSIZE];
    char originInfo[MSGBUFFSIZE];
    char *dataType = NULL;
    struct sigaction act, oldact;

    printf("Child: SGSeventhandler up\n");
    

    id = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);
    if(id == -1)
    {
        printf("Open event queue failed...\n");
        exit(0);
    }

    act.sa_handler = (__sighandler_t)ShutdownSystemBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    //Ignore SIGINT
    
    signal(SIGINT, SIG_IGN);

    sgsMasterId = sgsCreateMsgQueue(SGSKEY, 0);
    if(sgsMasterId == -1)
    {
        printf("Open master queue failed...\n");
        exit(0);
    }

    sgsSendQueueMsg(sgsMasterId, "[BOOT];EventHandler", EnumEventHandler);

    //Open up mail agent

    mailAgentMsgId = sgsCreateMsgQueue(MAIL_AGENT_KEY, 0);
    if(mailAgentMsgId == -1)
    {

        printf("Open mailAgent queue failed...\n");

    }
    else // We open mail agent only if we have the queue message been opened successfully
    {

        mailAgentPid = fork();
        if(mailAgentPid < 0)
        {
            perror("mailAgent");
        }
        else if(mailAgentPid == 0)
        {

            execlp(MAIL_AGENT_PATH,MAIL_AGENT_PATH,NULL);
            perror("mailAgent");
            exit(0);

        }

    }

   
    while(1)
    {

        memset(buf,'\0',sizeof(buf));
        memset(originInfo,'\0',sizeof(originInfo));

        //Prepare to receive message

        ret = sgsRecvQueueMsg(id,buf,0);

        strncpy(originInfo, buf, sizeof(originInfo));

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

                CheckLogFileSize("./log/SGSlog");
                AddToLogFile("./log/SGSlog",originInfo);

            }
            else if(!strcmp(dataType,ERROR))
            {

                SendMailToMaintainer(ret, originInfo);
                CheckLogFileSize("./log/SGSError");
                AddToLogFile("./log/SGSError",originInfo);

            }
            else if(!strcmp(dataType,BOOT))
            {

                sgsSendQueueMsg(sgsMasterId, originInfo, ret);
                CheckLogFileSize("./log/SGSlog");
                AddToLogFile("./log/SGSlog",originInfo);

            }
            else
            {

                printf("SGSEventhandler got message: %s, transmit to Master\n",originInfo);

                sgsSendQueueMsg(sgsMasterId, originInfo, ret);
                
            }

        }
            
        usleep(5000);

    }

    return 0;

}

void ShutdownSystemBySignal(int sigNum)
{

    kill(mailAgentPid, SIGTERM);

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

    fprintf(fp,"[%04d-%02d-%02d %02d:%02d:%02d],%s\n"
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

        fp = fopen(filePath,"w");
        fprintf(fp,"No log file detected. Create one here\n");
        fclose(fp);
        return 0;

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
        snprintf(message,1499,"EnumDataBuffer,%s", content);
        break;

        case EnumCollector:
        snprintf(message,1499,"EnumCollector,%s", content);
        break;

        case EnumUploader:
        snprintf(message,1499,"EnumUploader,%s", content);
        break;

        case EnumLogger:
        snprintf(message,1499,"EnumLogger,%s", content);
        break;

        default:
        printf(LIGHT_RED"Unknown msgtype %d\ncontent:%s\n"NONE,msgType,content);
        snprintf(message,1499,"Unknown,%s", content);
        break;

    }

    sgsSendQueueMsg(mailAgentMsgId, message, EnumEventHandler);
    return 0;

}
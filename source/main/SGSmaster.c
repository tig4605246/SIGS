/*

    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: July 13,2017
    Program statement: 
        Main master, Receive commands from server and issue controlling messages to all sub masters

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
#include <sys/wait.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"

//This structure is used for storing child process info

typedef struct{

    int pid;
    int msgId;
    char childName[32];

}childProcessInfo;

//Intent    : Initialize child and the message queue
//Pre       : Nothing
//Post      : On success, return message queue id, otherwise return -1 and set sgsErrNum

int InitChild(childProcessInfo *infoPtr, char *childPath, int key, char *name);

//Intent    : Check if children are still alive. If the child is dead, reap its old pid and restart it.
//Pre       : Nothing
//Post      : Nothing

void CheckChildAlive();

//Intent    : Restart the system and send the [message] to the maintainer by email
//Pre       : char pointer to the message
//Post       : Nothing

void RestartSystem(childProcessInfo *infoPtr, char *message);

//Intent    : Shutdown the system and send the shutdown message to the maintainer by email
//Pre       : char pointer to the message
//Post      : Nothing

void ShutdownSystem(childProcessInfo *infoPtr, char *message);

//Intent    : Shutdown the system, mainly for debugging
//Pre       : signal number
//Post      : Nothing

void ShutdownSystemBySignal(childProcessInfo *infoPtr, int sigNum);

//Intent    : Shutdown the system, mainly for debugging
//Pre       : child info struct
//Post      : Nothing

void ShutdownSystemByInput();

//Intent    : send routine log messages to the server
//Pre       : Nothing
//Post      : On success, return 0. Otherwise return -1 and set the ErrNum

int ReportToServer();

//All info about masters are here, be caredul with it

childProcessInfo cpInfo[5];

int main(int argc, char *argv[])
{

    int i, ret = 0;                             //  functional variables
    int sgsMasterId = -1;                       //  master's queue id
    char input[128];                            //  input buffer for manual mode
    char buf[MSGBUFFSIZE];                      //  buffer used to catch queue message
    char *msgType = NULL, *msgContent = NULL;   //  for process messages
    struct sigaction act, oldact;               //  for sigaction

    //Help

    if(argc < 2 || !strcmp(argv[1],"-h"))
    {

        printf("Usage: SGSmaster [options]\n");
        printf("Options:\n");
        printf("  -m     manual mode\n");
        printf("  -a     auto mode\n");
        printf("  -p     show default config path\n");
        exit(0);

    }

    //Initialize cpInfo

    for(i = 0 ; i < 5 ; i++)
    {

        cpInfo[i].pid = -1;
        cpInfo[i].msgId = -1;
        memset(cpInfo[i].childName,'\0',sizeof(cpInfo[i].childName));

    }
    
    //Version Info

    printf(LIGHT_GREEN"Starting SGSmaster...\n"NONE);

    showVersion();

    //Store own pid

    ret = sgsInitControl("SGSmaster");

    //Starting childs

    ret = InitChild(&(cpInfo[0]), EVENT_HANDLER_PATH, EVENT_HANDLER_KEY, "EventHandler");
    if(ret == -1)
    {

        ShutdownSystemByInput();

    }
    //ret = InitChild(cpInfo[1], DATABUFFER_SUBMASTER_PATH, DATABUFFER_SUBMASTER_KEY, "DataBufferSubmaster");
    //ret = InitChild(cpInfo[2], COLLECTOR_SUBMASTER_PATH, COLLECTOR_SUBMASTER_KEY, "CollectorSubmaster");
    //ret = InitChild(cpInfo[3], UPLOADER_SUBMASTER_PATH, UPLOADER_SUBMASTER_KEY, "UploaderSubmaster");
    //ret = InitChild(cpInfo[4], LOGGER_PATH, LOGGER_KEY, "Logger");

    //Set signal action

    act.sa_handler = (__sighandler_t)ShutdownSystemByInput;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGINT, &act, &oldact);

    //Open Master's message queue

    sgsMasterId = sgsCreateMsgQueue(SGSKEY, 1);
    if(sgsMasterId == -1)
    {
        printf("Open master queue failed...\n");
    }

    //Main loop

    if(!strcmp(argv[1],"-m"))   //Manual
    {
        while(1)
        {
            
            memset(input,'\0',sizeof(input));
            printf("$-> ");
            scanf(" %s",input);

            if(!strcmp(input,"bye"))
            {

                ShutdownSystemByInput(cpInfo);

            }
            else if(!strcmp(input,"hello-all"))
            {

                for(i = 0 ; i < 1 ; i++)
                {

                    memset(input,'\0',sizeof(input));
                    snprintf(input,127,"Hello %s",cpInfo[i].childName);
                    sgsSendQueueMsg(cpInfo[i].msgId, input, 1);
                
                }
                
            }
            else if(!strcmp(input,"mail-test"))
            {

                sgsSendEmail("Test mail by master");

            }

            printf("\njob done\n");
            usleep(100000);

        }
    }
    else if(!strcmp(argv[1],"-a"))  //Auto
    {

        i=0;
        while(1)
        {
            
            memset(buf,'\0',sizeof(buf));
            ret = sgsRecvQueueMsg(sgsMasterId,buf,1);

            if(ret != -1)
            {

                switch(ret)
                {

                    case EnumEventHandler:
                    sgsSendQueueMsg(cpInfo[0].msgId, input, EnumEventHandler);
                    break;

                    case EnumDataBuffer:
                    sgsSendQueueMsg(cpInfo[1].msgId, input, EnumDataBuffer);
                    break;

                    case EnumCollector:
                    sgsSendQueueMsg(cpInfo[2].msgId, input, EnumCollector);
                    break;

                    case EnumUploader:
                    sgsSendQueueMsg(cpInfo[3].msgId, input, EnumUploader);
                    break;

                    case EnumLogger:
                    sgsSendQueueMsg(cpInfo[4].msgId, input, EnumLogger);
                    break;

                    default:
                    printf(LIGHT_RED"Unknown msgtype %d\ncontent:%s\n"NONE,ret,buf);
                    memset(input,'\0',sizeof(input));
                    snprintf(input,127,"Master got an unknown type message");
                    sgsSendEmail(input);
                    break;

                }


            }

            usleep(100000);

            i++;

            if(i>50)
            {

                CheckChildAlive();
                i = 0;

            }

        }

    }
  
    return 0;

}

int InitChild(childProcessInfo *infoPtr, char *childPath, int key, char *name)
{

    int eventPid = 0;
    int id = -1;
    int i = 0, ret = 0;
    char buf[MSGBUFFSIZE];

    id = sgsCreateMsgQueue(key, 1);

    if(id == -1 )
    {

        printf("msg queue open failed, return -1");
        return -1;

    }

    eventPid = fork();

    if(eventPid == -1)
    {

        printf("fork error,return -1");
        return -1;
    
    }

    if(eventPid == 0)
    {

        execlp(childPath,childPath,NULL);

    }

    infoPtr->pid = eventPid;
    infoPtr->msgId = id;
    strncpy(infoPtr->childName,name,31);

    memset(buf,'\0',sizeof(buf));

    for(i = 0; i < 10 ; i++)
    {
        ret = sgsRecvQueueMsg(id,buf,2);
        if(ret == 0)
        {

            if(!strcmp(buf,infoPtr->childName))
            {

                printf("Child %s is opended\n",infoPtr->childName);
                return 0;

            }
            else
            {

                printf("message is : %s, fail to open target child\n",buf);
                //Get rid of the wrong child
                kill(infoPtr->pid,SIGTERM);
                do
                {

                    ret = waitpid((infoPtr)->pid, NULL, WNOHANG);
                    if(ret == 0)
                    {

                        printf("No child exited\n");

                    }
                    sleep(1);

                }while(ret == 0);
                return -1;
            
            }

        }

    }
    printf("Encounter Problem while initializing childs\n");
    return -1;

}

void ShutdownSystemByInput()
{

    int ret = 0, i = 0;
    FILE *ptr = NULL;
    childProcessInfo *infoPtr = cpInfo;
    printf("shutdown by input\n");

    for(i = 0 ; i < 5 ; i++)
    {

        if((infoPtr + i)->pid != -1)
        {

            kill((infoPtr + i)->pid, 9);

            do
            {

                ret = waitpid((infoPtr + i)->pid, NULL, WNOHANG);
                if(ret == 0)
                {

                    printf("No child exited\n");

                }
                sleep(1);

            }while(ret == 0);

            if(ret == (infoPtr + i)->pid )
            {

                printf("Catch Child %d successfully\n",ret);

            }
            else
            {

                printf("ret = %d, failed to catch the child\n",ret);

            }

            ret = sgsDeleteMsgQueue((infoPtr + i)->msgId);

            if(ret != 0)
            {

                printf(LIGHT_RED"[%s,%d] %s's Message queue deletion failed!!\n"NONE,__FUNCTION__,__LINE__,(infoPtr + i)->childName);

            }

        }
        

    }

    printf("Master Leaving...\n");

    exit(0);

}

void CheckChildAlive()
{

    childProcessInfo *infoPtr = cpInfo;
    char buf[128];
    char message[640];
    int i = 0, ret = 0;

    for(i = 0 ; i < 5 ; i++)
    {

        if((infoPtr + i)->pid != -1)
        {

            ret = waitpid((infoPtr + i)->pid, NULL, WNOHANG);
            if(ret != 0)
            {

                memset(buf, '\0', sizeof(buf));
                strncpy(buf, (infoPtr + i)->childName, 127);
                (infoPtr + i)->pid = -1;

                switch(i)
                {

                    case 0:
                    ret = InitChild((infoPtr + i), EVENT_HANDLER_PATH, EVENT_HANDLER_KEY, buf);
                    if(ret != 0)
                    {

                        printf("Try to restart %s but failed\n",buf);

                    }
                    break;

                    case 1:
                    ret = InitChild((infoPtr + i), DATABUFFER_SUBMASTER_PATH, DATABUFFER_SUBMASTER_KEY, buf);
                    if(ret != 0)
                    {

                        printf("Try to restart %s but failed\n",buf);

                    }
                    break;

                    case 2:
                    ret = InitChild((infoPtr + i), COLLECTOR_SUBMASTER_PATH, COLLECTOR_SUBMASTER_KEY, buf);
                    if(ret != 0)
                    {

                        printf("Try to restart %s but failed\n",buf);

                    }
                    break;

                    case 3:
                    ret = InitChild((infoPtr + i), UPLOADER_SUBMASTER_PATH, UPLOADER_SUBMASTER_KEY, buf);
                    if(ret != 0)
                    {

                        printf("Try to restart %s but failed\n",buf);

                    }
                    break;

                    case 4:
                    ret = InitChild((infoPtr + i), LOGGER_PATH, LOGGER_KEY, buf);
                    if(ret != 0)
                    {

                        printf("Try to restart %s but failed\n",buf);

                    }
                    break;

                    default:
                    break;

                }
                

            }

        }
        else
        {

            memset(buf, '\0', sizeof(buf));
            strncpy(buf, (infoPtr + i)->childName, 127);

            switch(i)
            {

                case 0:
                ret = InitChild((infoPtr + i), EVENT_HANDLER_PATH, EVENT_HANDLER_KEY, buf);
                if(ret != 0)
                {

                    printf("Try to restart %s but failed\n",buf);

                }
                break;

                case 1:
                ret = InitChild((infoPtr + i), DATABUFFER_SUBMASTER_PATH, DATABUFFER_SUBMASTER_KEY, buf);
                if(ret != 0)
                {

                    printf("Try to restart %s but failed\n",buf);

                }
                break;

                case 2:
                ret = InitChild((infoPtr + i), COLLECTOR_SUBMASTER_PATH, COLLECTOR_SUBMASTER_KEY, buf);
                if(ret != 0)
                {

                    printf("Try to restart %s but failed\n",buf);

                }
                break;

                case 3:
                ret = InitChild((infoPtr + i), UPLOADER_SUBMASTER_PATH, UPLOADER_SUBMASTER_KEY, buf);
                if(ret != 0)
                {

                    printf("Try to restart %s but failed\n",buf);

                }
                break;

                case 4:
                ret = InitChild((infoPtr + i), LOGGER_PATH, LOGGER_KEY, buf);
                if(ret != 0)
                {

                    printf("Try to restart %s but failed\n",buf);

                }
                break;

                default:
                break;

            }

        }
        

    }

}
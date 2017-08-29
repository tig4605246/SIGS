/*

    Name: Xi-Ping Xu
    Date: July 19,2017
    Last Update: July 19,2017
    Program statement: 
        This program manages collector agents, including
        1. watchdog
        2. tranfer commands from/to agents

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"

#define AUTOLIST "./conf/Collect/AutoList"

//sigaction
//Pid store
//Read Setting
//invoke agents
//Record their info
//While loop
//Send message to eventhandler

typedef struct{

    int pid;//child id
    int msgId;//child queue id, We are not using it here
    char childName[32];//Name of child process
    char childPath[64];//Path to child process

}childProcessInfo;

//Intent    :   Shutdown children after catching the SIGTERM
//Pre       :   signal number
//Post      :   Nothing


void ShutDownBySignal(int sigNum);

childProcessInfo cpInfo[MAXBUFFERINFOBLOCK]; //  All info about sub-masters are here, be caredul with it
int agentMsgId = -1;

int main()
{

    int ret = -1;
    int eventHandlerId = -1;    //  id for message queue to Event-Handler
    int collectorMasterId = -1;
    int i = 0, count = 10;
    pid_t pid;
    char buf[MSGBUFFSIZE];
    char originInfo[MSGBUFFSIZE];
    char *path = NULL;          // path to agent
    char *name = NULL;          // name of agent
    char *msgType = NULL;       //message type
    char *from = NULL;          //who issue this message
    char *to = NULL;          //who receive this message
    FILE *fp = NULL;
    struct sigaction act, oldact; 

    printf("Child: SGSdatabuffermaster up\n");

    eventHandlerId = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);
    if(eventHandlerId == -1)
    {
        printf("Open eventHandler queue failed...\n");
        exit(0);
    }

    collectorMasterId = sgsCreateMsgQueue(COLLECTOR_SUBMASTER_KEY, 0);
    if(collectorMasterId == -1)
    {
        printf("Open collector queue failed...\n");
        exit(0);
    }

    agentMsgId = sgsCreateMsgQueue(COLLECTOR_AGENT_KEY, 0);
    if(agentMsgId == -1)
    {
        printf("Open collector agent queue failed...\n");
        exit(0);
    }



    //Set signal action

    act.sa_handler = (__sighandler_t)ShutDownBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    //Ignore SIGINT
    
    signal(SIGINT, SIG_IGN);

    sgsSendQueueMsg(eventHandlerId, "[BOOT];CollectorSubmaster", EnumCollector);

    //Initialize child list

    for(i = 0 ; i < 5 ; i++)
    {

        cpInfo[i].pid = -1;
        cpInfo[i].msgId = -1;
        memset(cpInfo[i].childName, '\0', sizeof(cpInfo[i].childName));
        memset(cpInfo[i].childPath, '\0', sizeof(cpInfo[i].childPath));


    }

    //Read in list of auto start agent

    fp = fopen(AUTOLIST, "r");

    if(fp == NULL)
    {

        printf("No auto start list, skiping.\n");

    }
    else
    {

        //Open collect agents

        i = 0;
        while(i < MAXBUFFERINFOBLOCK || !feof(fp))
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
            
            if(path != NULL && name != NULL)
            {

                snprintf(cpInfo[i].childPath, 63, "%s", path);
                strncpy(cpInfo[i].childName, name, 31);

            }
            else
            {

                printf("[%s] is not a valid format\n",buf);
                continue;

            }

            //Invoke children

            cpInfo[i].pid = fork();
            if(cpInfo[i].pid == 0)
            {

                memset(buf,'\0',sizeof(buf));
                snprintf(buf,MSGBUFFSIZE,"%d",i);
                execlp(cpInfo[i].childPath, cpInfo[i].childPath, buf, NULL);
                perror("fork()");
                exit(0);

            }

            usleep(5000);

            //Check the child status right after the child starts

            ret = waitpid(cpInfo[i].pid, NULL, WNOHANG);

            if(ret != 0)
            {

                printf("%s;pid %d Agent %s exits unexpectedly, return code %d\n", ERROR, cpInfo[i].pid, cpInfo[i].childName, ret);
                memset(buf,'\0',sizeof(buf));
                snprintf(buf,511,"%s;[%s,%d]Collector-Master: pid %d Agent %s exits unexpectedly, return code %d"
                                                                    , ERROR, __FUNCTION__, __LINE__, cpInfo[i].pid, cpInfo[i].childName, ret);
                sgsSendQueueMsg(eventHandlerId, buf, EnumCollector);
                cpInfo[i].pid = -1;
                memset(cpInfo[i].childName, '\0', sizeof(cpInfo[i].childName));

                //If we add a continue here, we can avoid unused block between two used blocks

            }

            //Next child list

            i++;

        }

    }



    //In this infinity loop, we'll deal with messages

    while(1)
    {

        usleep(5000);
        memset(buf,'\0',sizeof(buf));
        ret = sgsRecvQueueMsg(collectorMasterId, buf, EnumCollector);

        //Message type: Restart | Leave | Error | Control | Log
        //
        ret = -1;
        if(ret != -1)
        {

            printf("CollectMaster got message:\n%s\n",buf);

            memset(originInfo,'\0',sizeof(originInfo));
            strncpy(originInfo, buf, sizeof(originInfo));

            msgType = strtok(buf,";");

            if(!strcmp(msgType,LOG))
            {

                sgsSendQueueMsg(eventHandlerId, buf, EnumCollector);

            }
            else if(!strcmp(msgType,ERROR))
            {

                sgsSendQueueMsg(eventHandlerId, buf, EnumCollector);

            }
            else if(!strcmp(msgType,LEAVE))
            {

                //Get process name

                from = strtok(NULL,";");

                //If we get the name

                if(from != NULL)
                {

                    //Which cpInfo is its

                    for(i=0;i<5;i++)
                    {

                        if(!strcmp(cpInfo[i].childName,from))
                        {

                            // Try to reap its exit signal

                            count = 10;

                            do
                            {

                                ret = waitpid(cpInfo[i].pid, NULL, WNOHANG);
                                count--;

                            }while(ret == 0 && count > 0);

                            //If we didn't get the signal in 10 loops

                            if(count <= 0)
                            {

                                printf("[%s]Timeout when waiting child %s quit\n",LEAVE ,from);

                            }

                            //Reset the info

                            memset(cpInfo[i].childName,'\0',sizeof(cpInfo[i].childName));
                            memset(cpInfo[i].childPath,'\0',sizeof(cpInfo[i].childPath));
                            cpInfo[i].pid = -1;
                            cpInfo[i].msgId = -1;

                            break;

                        }

                    }
                
                }
                else
                {

                    printf("[%s]Can't find %s collector. Is it still there?",LEAVE ,from);

                }
                
            }
            else if(!strcmp(msgType,RESTART))
            {

                //Get process name

                from = strtok(NULL,";");

                //If we get the name

                if(from != NULL)
                {

                    for(i=0;i<5;i++)
                    {

                        if(!strcmp(cpInfo[i].childName,from))
                        {

                            //send terminated signal to it

                            kill(cpInfo[i].pid, SIGTERM);

                            // Try to catch its exit signal

                            count = 10;
                            do
                            {

                                ret = waitpid(cpInfo[i].pid, NULL, WNOHANG);
                                count--;

                            }while(ret == 0 && count > 0);

                            //If we fail the above task

                            if(count <= 0)
                            {

                                printf("[%s]Timeout when waiting child %s quit\n",RESTART ,from);

                            }

                            //fork() our child to restart the agent

                            cpInfo[i].pid = fork();

                            if(cpInfo[i].pid == 0)
                            {

                                execlp(cpInfo[i].childPath, cpInfo[i].childPath, i, NULL);
                                perror("fork()");
                                exit(0);

                            }

                            break;

                        }

                    }
                
                }
                else
                {

                    printf("[%s]Can't find %s collector. Is it still there?",RESTART ,from);

                }

            }
            else if(!strcmp(msgType,SHUTDOWN))
            {

                //Try to get process name

                from = strtok(NULL,";");

                //If we get the name

                if(from != NULL)
                {

                    //Find its cpInfo

                    for(i=0;i<5;i++)
                    {

                        //If we find it

                        if(!strcmp(cpInfo[i].childName,from))
                        {

                            //Send terminated signal to it

                            kill(cpInfo[i].pid, SIGTERM);

                            //Try to reap its exit signal

                            count = 10;
                            do
                            {

                                ret = waitpid(cpInfo[i].pid, NULL, WNOHANG);
                                count--;

                            }while(ret == 0 && count > 0);


                            //If we fail the above task

                            if(count <= 0)
                            {

                                printf("[%s]Timeout when waiting child %s quit\n",RESTART ,from);

                            }

                            //Reset the cpInfo

                            memset(cpInfo[i].childName,'\0',sizeof(cpInfo[i].childName));
                            cpInfo[i].pid = -1;
                            cpInfo[i].msgId = -1;

                        }

                    }

                }

            }
            else if(!strcmp(msgType,CONTROL))
            {

                //Try to get process name

                to = strtok(NULL,";");

                //If we get the name

                if(to != NULL)
                {

                    //Find its cpInfo

                    for(i=0;i<5;i++)
                    {

                        //If we find it, send the original message to it

                        if(!strcmp(cpInfo[i].childName,to))
                        {

                            sgsSendQueueMsg(agentMsgId, originInfo, i);
                            break;

                        }

                    }

                    //If we didn't find the target we can do something here

                }

            }
            else if(!strcmp(msgType,RESULT))
            {

                //Try to get process name

                to = strtok(NULL,";");

                //If we get the name

                if(to != NULL)
                {

                    //Find its cpInfo

                    for(i=0;i<5;i++)
                    {

                        //If we find it, send the original message to it

                        if(!strcmp(cpInfo[i].childName,to))
                        {

                            sgsSendQueueMsg(agentMsgId, originInfo, i);
                            break;

                        }

                    }

                    //If we didn't find the target we can do something here

                }

            }
            else//for other messages we don't know the purpose
            {

                sgsSendQueueMsg(eventHandlerId, originInfo, EnumCollector);

            }

        }

    }

}


void ShutDownBySignal(int sigNum)
{

    int i = 0;

    printf("Shuting down collector children before bye bye\n");

    for(i = 0 ; i < 5 ; i++)
    {

        if(cpInfo[i].pid != -1)
        {

            kill(cpInfo[i].pid,SIGTERM);

        }

    }

    exit(0);
    return;

}
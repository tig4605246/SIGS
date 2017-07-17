#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"

//sigaction
//Pid store
//invoke child
//Record their info
//Send message to eventhandler

typedef struct{

    int pid;
    int msgId;
    char childName[32];

}childProcessInfo;

void ShutDownBySignal(int sigNum);

childProcessInfo cpInfo[5]; //  All info about sub-masters are here, be caredul with it

int main()
{

    int ret = -1;
    int id = -1;//  id for message queue to event-handler
    int uploadMasterId = -1;
    int i = 0;
    pid_t pid;
    char buf[512];
    struct sigaction act, oldact; 

    printf("Child: SGSdatabuffermaster up\n");

    id = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);
    uploadMasterId = sgsCreateMsgQueue(UPLOADER_SUBMASTER_KEY, 0)

    //Set signal action

    act.sa_handler = (__sighandler_t)ShutDownBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGINT, &act, &oldact);


    for(i = 0 ; i < 5 ; i++)
    {

        cpInfo[i].pid = -1;
        cpInfo[i].msgId = -1;
        memset(cpInfo[i].childName, '\0', sizeof(cpInfo[i].childName));

    }

    //Open upload agents

    for(i = 0 ; i < 5 ; i++)
    {

        cpInfo[i].pid = fork();
        if(cpInfo[i].pid == 0)
        {

            execlp(CHILD_PATH, CHILD_PATH, NULL);
            perror("fork()");
            exit(0);

        }

    }

    while(1)
    {

        usleep(50000);
        memset(buf,'\0',sizeof(buf));
        ret = sgsRecvQueueMsg(uploadMasterId, buf, EnumUploader);
        if(ret != -1)
        {

            //

        }


    }


}


void ShutDownBySignal(int sigNum)
{

    printf("Shuting down child before bye bye\n");
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
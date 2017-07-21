/*

    Name: Xi-Ping Xu
    Date: July 5,2017
    Last Update: July 21,2017
    Program statement: 
        its only task is sending emails

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"

void ShutdownSystemBySignal(int sigNum);

int main(int argc, char *argv[])
{

    int mailAgentMsgId = -1, ret = -1;
    char buf[MSGBUFFSIZE];
    struct sigaction act, oldact;


    act.sa_handler = (__sighandler_t)ShutdownSystemBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    mailAgentMsgId = sgsCreateMsgQueue(MAIL_AGENT_KEY, 0);
    if(mailAgentMsgId == -1)
    {

        printf(LIGHT_RED"Failed to open mailAgent message queue with key %d\n"NONE,MAIL_AGENT_KEY);
        exit(0);

    }

    while(1)
    {

        usleep(100000);
        memset(buf,'\0',sizeof(buf));
        ret = sgsRecvQueueMsg(mailAgentMsgId, buf, 0);
        if(ret != -1)
        {

            printf(YELLOW"MailAgent got message type %d\n"NONE, ret);
            sgsSendEmail(buf);

        }

    }

}

void ShutdownSystemBySignal(int sigNum)
{

    printf("MailAgent bye bye\n");
    exit(0);

}

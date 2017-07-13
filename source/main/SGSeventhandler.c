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

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"

#define CMDLEN 128
#define BUFLEN 2048

void ShutdownSystemBySignal(int sigNum);

int main()
{

    int eventPid = 0;
    int id = -1;
    int ret;
    char buf[MSGBUFFSIZE];
    struct sigaction act, oldact;

    printf("Child: SGSeventhandler up\n");

    id = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);

    act.sa_handler = (__sighandler_t)ShutdownSystemBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    while(1)
    {

        ret = sgsRecvQueueMsg(id,buf,1);

        if(ret != -1 )
        {

            printf("Child got message: %s\n",buf);
            
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
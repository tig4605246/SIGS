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

int main()
{

    int eventPid = 0;
    int id = -1;
    int ret;
    char buf[1024];

    id = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);

    while(1)
    {

        ret = sgsRecvQueueMsg(id,buf,1);


    }

}
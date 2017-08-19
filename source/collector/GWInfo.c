/*
    Name: Xu Xi-Ping
    Date: March 13,2017
    Last Update: July 27,2017
    Program statement: 
        This program will gather gateway's infos and updates the data in the shared memory

        Gateway info includes:

        1.CPU usage
        2.MEM usage
        3.Storage usage
        4.network flow
        

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/timeb.h>

#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

//We declare our own libraries at below

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"


#define CPUFILE "/proc/stat"
#define MEMFILE "cat /proc/meminfo"
#define DISKFILE "df /dev/sda5"

#define NETFLOWFILE "/proc/net/dev" // path of network log
#define NETFILEEXT "./log/NetworkFlow" // path of existing network log

int CollectCpuUsage(dataInfo *target);

int CollectDiskUsage(dataInfo *target);

int CollectMemoryUsage(dataInfo *target);

int CollectNetworkFlow(dataInfo *target);

int CheckAndRespondQueueMessage();

void ShutdownSystemBySignal();

typedef struct rxRead
{
    unsigned long long int bytes, packg, errs, drop, fifo, frame, compress, multicast;

}rxRead;

typedef struct txRead
{
    unsigned long long int bytes, packg, errs, drop, fifo, colls, carrier, compress;

}txRead;

typedef struct jiffy_counts_t 
{

    /* Linux 2.4.x has only first four */

    unsigned long long usr, nic, sys, idle;
    unsigned long long iowait, irq, softirq, steal;
    unsigned long long total;
    unsigned long long busy;

} jiffy_counts_t;

typedef struct df_filesystem_space_t 
{

    unsigned char Filesystem[32];
    unsigned long long blocks_1K;
    unsigned long long Used;
    unsigned long long Available;
    unsigned int Use;

} df_filesystem_space_t;

dataInfo *dInfo = NULL;
int interval = 10;  // time period between last and next collecting section
int eventHandlerId; // Message queue id for event-handler
int shmId;          // shared memory id
int msgId;          // created by sgsCreateMessageQueue
int msgType;        // 0 1 2 3 4 5, one of them

char netCard[16] = {'\0'};


int main(int argc, char *argv[])
{

    int i = 0, ret = 0, numberOfData = 0;
    char buf[512];
    FILE *fp = NULL;
    time_t last, now, netPeriod;
    dataInfo *temp = NULL;
    struct sigaction act, oldact;

    printf("GWInfo starts---\n");

    memset(buf,'\0',sizeof(buf));

    snprintf(buf,511,"%s;argc is %d, argv 1 %s\n", LOG, argc, argv[1]);

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

    msgId = sgsCreateMsgQueue(COLLECTOR_AGENT_KEY, 0);
    if(msgId == -1)
    {
        printf("Open Collector Agent queue failed...\n");
        exit(0);
    }

    dInfo = NULL;
    shmId = -1;

    ret = sgsInitDataInfo(NULL, &dInfo, 0, "./conf/Collect/SolarCollector", -1, &numberOfData);

    if(ret < 0 )
    {

        printf("failed to create dataInfo, ret is %d\n",ret);
        sgsSendQueueMsg(eventHandlerId,"[Error];failed to create dataInfo",9);
        exit(0);

    }

    printf("ret return %d, data number %d\n", ret, numberOfData);

    //Store shared memory id

    shmId = ret;

    //get first timestamp

    time(&last);
    now = last;
    netPeriod = now -  605;

    //main loop

    while(1) 
    {

        usleep(100000);
        time(&now);

        //check time interval

        if((now-last) > interval + 2  )
        {

            temp = dInfo;

            while(temp != NULL)
            {

                //Update data
                //printf("Getting system info\n");

                if(!strcmp(temp->valueName, "CPU_Usage"))
                {

                    printf("Getting CPU\n");
                    ret = CollectCpuUsage(temp);

                    if(ret != 0)
                    {

                        memset(buf,0,sizeof(buf));
                        snprintf(buf, sizeof(buf) - 1, "%s;Failed to get cpu usage", ERROR);
                        sgsSendQueueMsg(eventHandlerId, buf, msgId);


                    }

                }
                else if(!strcmp(temp->valueName, "Memory_Usage")  )
                {

                    printf("Getting MEM\n");
                    
                    ret = CollectMemoryUsage(temp);

                    if(ret != 0)
                    {

                        memset(buf,0,sizeof(buf));
                        snprintf(buf, sizeof(buf) - 1, "%s;Failed to get memory usage", ERROR);
                        sgsSendQueueMsg(eventHandlerId, buf, msgId);


                    }

                }
                else if(!strcmp(temp->valueName, "Storage_Usage"))
                {

                    printf("Getting HDD\n");
                    ret = CollectDiskUsage(temp);

                    if(ret != 0)
                    {

                        memset(buf,0,sizeof(buf));
                        snprintf(buf, sizeof(buf) - 1, "%s;Failed to get storage usage", ERROR);
                        sgsSendQueueMsg(eventHandlerId, buf, msgId);


                    }

                }
                else if(!strcmp(temp->valueName, "Network_Flow") && ((now - netPeriod) > 600))
                {

                    printf("Getting Net Flow\n");
                    netPeriod = now;
                    ret = CollectNetworkFlow(temp);

                    if(ret != 0)
                    {

                        memset(buf,0,sizeof(buf));
                        snprintf(buf, sizeof(buf) - 1, "%s;Failed to get Network flow", ERROR);
                        sgsSendQueueMsg(eventHandlerId, buf, msgId);

                    }

                }
                temp = temp->next;

            }
            time(&last);
            now = last;
            last -= 2;

        }

        //Check message

        ret = CheckAndRespondQueueMessage();

    }

}

int CollectCpuUsage(dataInfo *target) 
{

    FILE *fp = NULL;
    dataLog data;
    float cpu_use,mem_use;
    unsigned char buf[4096];
    jiffy_counts_t p_jif, c_jif;
    static const char fmt[] = "cpu %llu %llu %llu %llu %llu %llu %llu %llu";    

    printf("[%s:%d] Function started\n",__FUNCTION__, __LINE__);

    //Try to get CPU usage by reading cpu status in twice with small delay
    //Try to open up the cpu file

    fp = fopen(CPUFILE, "r");
    fgets(buf, 4096, fp);
    fclose(fp);
    sscanf(buf, fmt, 
                &p_jif.usr, &p_jif.nic, &p_jif.sys, &p_jif.idle,
                &p_jif.iowait, &p_jif.irq, &p_jif.softirq,
                &p_jif.steal);

    p_jif.total = p_jif.usr + p_jif.nic + p_jif.sys + p_jif.idle;

    sleep(5);

    fp = fopen(CPUFILE, "r");
    fgets(buf, 4096, fp);
    fclose(fp);

    sscanf(buf, fmt, 
                &c_jif.usr, &c_jif.nic, &c_jif.sys, &c_jif.idle,
                &c_jif.iowait, &c_jif.irq, &c_jif.softirq,
                &c_jif.steal);
    c_jif.total = c_jif.usr + c_jif.nic + c_jif.sys + c_jif.idle;

    //Caculate the cpu usage

    cpu_use = c_jif.total - p_jif.total;
    cpu_use = (float)((cpu_use - (c_jif.idle - p_jif.idle)) / cpu_use);

    if(cpu_use > 1) cpu_use = 1;
    else if(cpu_use < 0) cpu_use = 0;

    memset(&data,0,sizeof(data));
    data.value.i = (100 * cpu_use);
    data.valueType = INTEGER_VALUE;
    
    return sgsWriteSharedMemory(target,&data);

}

int CollectDiskUsage(dataInfo *target)
{

    int i = 0;
    dataLog data;

    struct timeb tp;
    FILE *fp = NULL;
    dataInfo *temp = NULL;
    unsigned char buf[4096];
    df_filesystem_space_t df;



    df.Use = 0;

    sprintf(buf, DISKFILE);
    fp = popen(buf, "r");

    //Get the info we want from the disk file

    while(fgets(buf, 4096, fp))
    {

        if(++i == 2)
        {

            sscanf(buf, "%s %llu %llu %llu %d",
                        df.Filesystem, &df.blocks_1K, &df.Used,
                        &df.Available, &df.Use);

            break;

        }

    }

    pclose(fp);

    // Store disk usage
    memset(&data,0,sizeof(data));
    data.value.i = (int )(df.Use);
    data.valueType = INTEGER_VALUE;

   

    // insert data to shared memory

    return sgsWriteSharedMemory(target, &data);

}

int CollectMemoryUsage(dataInfo *target) 
{

	int i;
    FILE *fp;
    dataLog data;
    
    float mem_use;
    dataInfo *temp = NULL;
    unsigned char buf[4096], buf2[32];
    unsigned long long memtotal, memfree, memavailable, membuffer, memcached;
    
    
	struct timeb tp;

    printf("[%s:%d] Function started\n",__FUNCTION__, __LINE__);


    //Try to read memory file

    i = 0;
    memtotal = 1;
    memfree = 1;
    sprintf(buf, MEMFILE);
    fp = popen(buf, "r");

    while(fgets(buf, 4096, fp))
    {

        i++;

        if(i == 1)
        {

            sscanf(buf, "%s %llu", buf2, &memtotal);

        }
        else if(i == 2)
        { //i==2

            sscanf(buf, "%s %llu", buf2, &memfree);
        
        }
        else if(i == 3)
        {

            sscanf(buf, "%s %llu", buf2, &memavailable);

        }
        else if(i == 4)
        {

            sscanf(buf, "%s %llu", buf2, &membuffer);

        }
        else if(i == 5)
        {

            sscanf(buf, "%s %llu", buf2, &memcached);

        }

    }

    pclose(fp);

    //Caculate memory usage

    mem_use = (float)((memtotal - memfree - membuffer - memcached )) / (float)memtotal;

    //store memory usage in % type
    memset(&data,0,sizeof(data));
    data.value.i = (100 * mem_use);
    data.valueType = INTEGER_VALUE;

    //Try to open disk file 

    
    return sgsWriteSharedMemory(target, &data);

}

int CollectNetworkFlow(dataInfo *target)
{

    /* ************************************************************ */

    FILE *fp = NULL;
    FILE *to = NULL;
    dataLog data;
    DATETIME tw_time;

    int count = 0;
    unsigned long long int tempRx, tempTx;
    static unsigned long long int var_temp_rx, var_temp_tx;

    static const char wread[]="wlp2s0: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu";
    static const char xread[]="%llu %llu";

    unsigned char buf[4096];

    rxRead rx;
    txRead tx;

    //Shows function has been called

    printf("[%s:%d] Function started\n",__FUNCTION__, __LINE__);

    //Try to open the network interfaces file

    to = fopen(NETFILEEXT, "r");

    if(!to)
    {

        printf(LIGHT_RED"[%s,%d] ERROR : open %s failed\n",__FUNCTION__,__LINE__,NETFILEEXT);

        var_temp_rx = 0;
        var_temp_tx = 0;

    }
    else
    {

        if(fgets(buf, 4096, to))
        {

            sscanf(buf, xread, &tempRx, &tempTx);

        }

        pclose(to);

        printf("first : %llu\n\n", tempRx);

    }

    //Get current TX RX

    fp = fopen(NETFLOWFILE, "r");

    if(!fp)
    {

        printf("[%s:%d] There is no such file or path is incorrect\n ",__FUNCTION__,__LINE__);
        memset(&rx,0,sizeof(rx));
        memset(&tx,0,sizeof(tx));

    }
    else
    {

        while(fgets(buf, 4096, fp))
        {

            if(count==3)        // Read data on line 4
            {

                sscanf(buf, wread,
                    &rx.bytes, &rx.packg, &rx.errs, &rx.drop, &rx.fifo, &rx.frame, &rx.compress, &rx.multicast,
                    &tx.bytes, &tx.packg, &tx.errs, &tx.drop, &tx.fifo, &tx.colls, &tx.carrier, &tx.compress);

            }
            else
            {
                count++;
            }

        }
        pclose(fp);

    }

    

    to = fopen(NETFILEEXT, "w");

    var_temp_rx  = tempRx;
    var_temp_tx  = tempTx;

    tempTx = (tx.bytes);
    tempRx = (rx.bytes);


    fprintf(to, xread, tempRx, tempTx);

    data.value.i = (tempRx - var_temp_rx) + (tempTx - var_temp_tx);
    data.valueType = INTEGER_VALUE;

    pclose(to);

    return sgsWriteSharedMemory(target,&data);

}

int CheckAndRespondQueueMessage()
{

    int ret = -1;
    char buf[MSGBUFFSIZE];
    char *cmd = NULL;
    char *to = NULL;
    char *from = NULL;
    char *storagePath = NULL;
    char *networkInterface = NULL;

    ret = sgsRecvQueueMsg(msgId, buf, msgType);

    if(ret != -1)
    {

        printf("GWInfo got message: %s\n", buf);

        cmd = strtok(buf, SPLITTER);
        to  = strtok(NULL, SPLITTER);
        from = strtok(NULL, SPLITTER);
        storagePath = strtok(NULL, SPLITTER);
        networkInterface = strtok(NULL, SPLITTER);    

    }
    

    return 0;


}

void ShutdownSystemBySignal()
{

    sgsDeleteDataInfo(dInfo, -1);
    printf("[%s,%d] Catched SIGUSR1 successfully\n",__FUNCTION__,__LINE__);
    exit(0);

    return;

}
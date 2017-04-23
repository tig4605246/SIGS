/*
    Name: Xu Xi-Ping
    Date: March 13,2017
    Last Update: March 13,2017
    Program statement: 
        This program will gather gateway's infos and updates the data in the shared memory
        
    Current Status:
        Lack of error handling machenism

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/timeb.h>

//We declare our own libraries at below

#include "../ipcs/SGSipcs.h"


#define CPUFILE "/proc/stat"
#define MEMFILE "cat /proc/meminfo"
#define DISKFILE "df /dev/sda5"

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

//the info of cpu mem and disk will be stored at here

int data[3] = {0};

int shmID = 0;

dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;

//Intent : Get Disk usage and write to target shm
//Pre : pointer to target shm area
//Post : On success, return 0. Otherwise return -1

int collectCpuUsage(dataInfo *target);

//Intent : Get Memory usage and write to target shm
//Pre : pointer to target shm area
//Post : On success, return 0. Otherwise return -1

int collectMemoryUsage(dataInfo *target);

//Intent : Get Disk usage and write to target shm
//Pre : pointer to target shm area
//Post : On success, return 0. Otherwise return -1

int collectDiskUsage(dataInfo *target);

//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();

//Intent : terminate the process correctly. It's called by the sigaction with signal SIGUSR1
//Pre : None
//Post : None

void stopAndAbort();


int main(int argc, char argv[])
{

    struct sigaction act, oldact;
    deviceInfo *temp = NULL;
    dataInfo *tmp = NULL;
    FILE *pidFile = NULL;
    int ret = 0;

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("[%s,%d] Failed to initialize the configure and shared memory, quitting\n",__FUNCTION__,__LINE__);

    }

    pidFile = fopen("./pid/GWInfo.pid","w");
    if(pidFile != NULL)
    {
        fprintf(pidFile,"%d",getpid());
        fclose(pidFile);
    }
    

    act.sa_handler = (__sighandler_t)stopAndAbort;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

    temp = deviceInfoPtr;
    
    //Get the dataInfoPtr we want

    while(temp != NULL)
    {

        if(strcmp(temp->deviceName,"GWInfo"))
            temp = temp->next;

        else
        {

            tmp = temp->dataInfoPtr;
            break;

        }

    }
    if(temp == NULL)
    {

        printf("Can't find GWInfo deviceInfo\n");
        return -1;

    }
    if(tmp == NULL)
    {

        printf("No dataInfo attached to the deviceInfo\n");
        return -1;

    }
    //Main Loop

    while(1)
    {

        tmp = temp->dataInfoPtr;
        while(tmp != NULL)
        {

            if(!strcmp(tmp->valueName,"CPU_Usage") )
            {
                printf("[%s,%d] Collect CPU USage\n",__FUNCTION__,__LINE__);
                ret = collectCpuUsage(tmp);
                if(ret < 0)
                    printf("collect CPU Usage failed\n");
            }
            
            else if(!strcmp(tmp->valueName,"Memory_Usage"))
            {
                printf("[%s,%d] Collect Memory USage\n",__FUNCTION__,__LINE__);
                ret = collectMemoryUsage(tmp);
            }
            else if(!strcmp(tmp->valueName,"Disk_Usage"))
            {
                printf("[%s,%d] Collect Disk USage\n",__FUNCTION__,__LINE__);
                ret = collectDiskUsage(tmp);
            }
            
            tmp = tmp->next;

        }
        sleep(10);
        

    }

    

    printf("\nWhatever SG INFO HERE\n");
    //sgsShowDeviceInfo(deviceInfoPtr);
    //sgsShowDataInfo(dataInfoPtr);

    return 0;

}

int collectCpuUsage(dataInfo *target) 
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

int collectDiskUsage(dataInfo *target)
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

int collectMemoryUsage(dataInfo *target) 
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

int initializeInfo()
{

    int ret = 0;
    ret = sgsInitDeviceInfo(&deviceInfoPtr);
    if(ret != 0)
    {

        printf("[%s,%d] init device conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    } 

    ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 0);
    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    shmID = ret;

    return 0;

}

void stopAndAbort()
{

    sgsDeleteAll(deviceInfoPtr, -1);
    printf("[%s,%d] Catched SIGUSR1 successfully\n",__FUNCTION__,__LINE__);
    exit(0);

    return;

}
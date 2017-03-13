/*
    Name: Xu Xi-Ping
    Date: March 13,2017
    Last Update: March 13,2017
    Program statement: 
        This program will gather gateway's infos and updates the data in the shared memory

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/timeb.h>

//We declare own libraries at below

#include "../ipcs/SGSipcs.h"


#define CPUFILE "/proc/stat"
#define MEMFILE "cat /proc/meminfo"
#define DISKFILE "df /dev/sda2"

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

//Intent : get CPU HDD MEM usage
//Pre : Nothing
//Post : Nothing


void collectSystemInfo();


int main(int argc, char argv[])
{


    return 0;

}



void collectSystemInfo() 
{

	int i;
    FILE *fp;
    DATETIME tw_time;
    float cpu_use,mem_use;
    df_filesystem_space_t df;
    jiffy_counts_t p_jif, c_jif;
    unsigned char buf[4096], buf2[32];
    static const char fmt[] = "cpu %llu %llu %llu %llu %llu %llu %llu %llu";
    unsigned long long memtotal, memfree, memavailable, membuffer, memcached;
    
    
	struct timeb tp;

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

    data[0] = (100 * cpu_use);

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

    data[1] = (100 * mem_use);

    //Try to open disk file 

    i = 0;
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

    data[2] = (df.Use);
    
    printf("[%s,%d]\nCPU %d \nMEM %d \nDISK %d\n",__FUNCTION__,__LINE__,data[0],data[1],data[2]);

    // Get current time

    ftime(&tp);
	tw_time = tp.time*1000 + tp.millitm;
	
    
    return;

}
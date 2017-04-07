/*

    Name: Xu Xi-Ping
    Date: March 28,2017
    Last Update: March 31,2017
    Program statement: 
        This program will collect info from cpm70_agent and update datas in shared memory
        
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
#include "../controlling/SGScontrol.h"


#define CMDLEN 128
#define BUFLEN 2048


deviceInfo *deviceInfoPtr = NULL;

dataInfo *dataInfoPtr = NULL;


//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

static int initializeInfo();

//Intent : execute cpm70_agent, parse data and store it to the shared memory
//Pre : Nothing
//Post : Nothing

static void getInfo();

//Intent : close program correctly
//Pre : signal number catched by sigaction
//Post : Nothing 

static void stopAndLeave(int sigNum);




int main(int argc, char *argv[])
{

    int ret = 0;
    struct sigaction act, oldact;
    

    //Recording program's pid

    ret = sgsInitControl("cpm70_agent");
    if(ret < 0)
    {

        printf("cpm70_agent aborting\n");
        return -1;

    }

    //Initialize deviceInfo and dataInfo

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("cpm70_agent aborting\n");
        return -1;

    }

    //Catch aborting signal

    act.sa_handler = (__sighandler_t)stopAndLeave;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

    while(1)
    {

        getInfo();
        sleep(5);

    }

    stopAndLeave(0);
    return 0;

}

static void getInfo()
{

    FILE *fpRead = NULL;
    char cmd[CMDLEN];
    char buf[BUFLEN];
    char* raw[34];
    char *tempPt = NULL;
    int i = 0;
    int ret = 0;

    deviceInfo *deviceTemp = deviceInfoPtr;
    dataInfo *dataTemp = NULL;
    dataLog data;

    //Ready the command at here

    memset(cmd,0,sizeof(sizeof(cmd)));

    snprintf(cmd,CMDLEN,"./cpm70-agent --get-dev-status");

    //execute command

    fpRead = popen(cmd,"r");

    //printf("%s\n",cmd);

    //Read result to buf

    fgets(buf, BUFLEN , fpRead);

    /*

        Process agent status here

    */

    while(strcmp(deviceTemp->deviceName,"cpm70_agent") && deviceTemp != NULL)
        deviceTemp = deviceTemp->next;

    

    //Read Detail to buf at this loop

    while(1)
    {

        //initialize pointer

        if(deviceTemp != NULL)
            dataTemp = deviceTemp->dataInfoPtr;

        //Initialize char pointer array

        for(i = 0 ; i < 33 ; i++)
            raw[i] = NULL;

        //flush input buffer

        memset(buf,'\0',sizeof(buf));

        //Get one sensor's info at a time

        fgets(buf, BUFLEN , fpRead);

        //Set the last char to be zero, prevents overflow

        buf[strlen(buf)] = '\0';

        //printf("buf len %lu\n%s",strlen(buf),buf);//debugging

        if(strlen(buf) != 0)
        {
            
            for(i = 0 ; i < 30 ; i++)
            {

                if(i == 0)
                {

                    raw[i] = strchr(buf,';');
                    if(raw[i] != NULL) 
                    {
                        *raw[i] = 0;
                        raw[i]++;
                    }

                }
                //printf("i = %d\n",i);//debugging

                raw[i+1] = strchr(raw[i],';');

                if(raw[i+1] != NULL) 
                {
                    *raw[i+1] = 0;
                    raw[i+1]++;
                }

            } 

            

            while(dataTemp != NULL && strcmp(dataTemp->sensorName,buf))
                dataTemp = dataTemp->next;


            //Check if I am parsing it right or not
            //Put them into shared memory 

            for(i = 0 ; i < 30 ; i++)
            {

                //printf("raw[%d] = %s\n",i,raw[i]);

                //Check if deviceTemp exists

                if(deviceTemp != NULL)
                {

                    //store ID (which is in the buf)

                    if(dataTemp != NULL && i == 0)
                    {

                        data.valueType = STRING_VALUE;
                        memset(data.value.s,'\0',sizeof(data.value.s));
                        strncpy(data.value.s, buf ,DATAVALUEMAX);
                        if(ret = sgsWriteSharedMemory(dataTemp, &data) != 0 )
                        {
                            printf("write to shared memory failed %d\n",i);
                        }
                        dataTemp = dataTemp->next;

                    }

                    if(dataTemp != NULL )
                    {

                        data.valueType = STRING_VALUE;
                        memset(data.value.s,'\0',sizeof(data.value.s));
                        strncpy(data.value.s, raw[i] ,DATAVALUEMAX);
                        if(ret = sgsWriteSharedMemory(dataTemp, &data) != 0 )
                        {
                            printf("write to shared memory failed %d\n",i);
                        }
                        dataTemp = dataTemp->next;

                    }
                    else
                    {

                        printf("Can't find matched sensorName\n");

                    }

                }

            }   
            
        }
        else
        {

            printf("Read done\n");

            break;

        }

    }

    //Always do clean up

    fclose(fpRead);
    return;

}

static int initializeInfo()
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

    

    return 0;

}

static void stopAndLeave(int sigNum)
{

    sgsDeleteAll(deviceInfoPtr, -1);
    printf("cpm70_agent is quitting...\n");
    exit(0);
    return;

}
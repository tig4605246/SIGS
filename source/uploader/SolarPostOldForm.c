/*

    Name: Xi-Ping Xu
    Date: July 19,2017
    Last Update: October 25,2017
    Program statement: 
        This is a agent used to test SGS system. 
        It has following functions:
        1. Get a data buffer info from the data buffer pool
        2. Show fake data.
        3. Issue Command to FakeTaida and receive result

    Update:
        2017/10/25:
            1. Change irr data into array

*/

/*

    Process:
    
    1. Init

    (Loop)
        
        2. Collect data and post to server
        3. Return value:
            i.      Changing config
            ii.     Resend
            iii.    Resend logs (with SolarPut)
        4.  Process queue message

*/

#define _GNU_SOURCE /* for tm_gmtoff and tm_zone */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <error.h>
#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"
#include "../thirdparty/cJSON.h"

//Flag for debugging

//#define DEBUG

//Post definitions for max length

#define SA      struct sockaddr
#define MAXLINE 16384
#define MAXSUB  16384

//Intent    : Get config from conf file. If failed, use the default one
//Pre       : Nothing
//Post      : Always return 0

int GetConfig();

//Intent    : Set config to conf file. If failed, discard the setting
//Pre       : Nothing
//Post      : On success, return 0, -1 means resend immediately

int SetConfig(char *result, char *address);

//Intent    : Post data to server
//Pre       : Nothing
//Post      : On success, return 0, otherwise return -1

int PostToServer();

ssize_t process_http( char *content, char *address);

int my_write(int fd, void *buffer, int length);

int my_read(int fd, void *buffer, int length);

//Intent    : Process queue message
//Pre       : Nothing
//Post      : Always return 0

int CheckAndRespondQueueMessage();

int ShutdownSystemBySignal(int sigNum);

dataInfo *dInfo[2] = {NULL,NULL};    // pointer to the shared memory
int interval = 60;  // time period between last and next collecting section
int eventHandlerId; // Message queue id for event-handler
int shmId;          // shared memory id
int msgId;          // created by sgsCreateMessageQueue
int msgType;        // 0 1 2 3 4 5, one of them

char errResult[256];

typedef struct postNode
{

    char GW_ver[16];        //string
    char IP[4][128];        //string
    epochTime Date_Time;    //time_t
    int Send_Rate;          //float to int
    int Gain_Rate;          //float to int
    int Backup_time;        //int
    char MAC_Address[32];   //string
    char Station_ID[16];    //string
    char GW_ID[16];         //string

}pNode;

pNode postConfig;

int main(int argc, char *argv[])
{

    int i = 0, ret = 0, numberOfData[2] = {0,0};
    char buf[512];
    FILE *fp = NULL;
    time_t last, now;
    struct sigaction act, oldact;
    DataBufferInfo bufferInfo[2];  

    struct sigaction sigchld_action = {
    .sa_handler = SIG_DFL,
    .sa_flags = SA_NOCLDWAIT
    };

    //This sigaction prevents zombie children while forking PUT agent

    sigaction(SIGCHLD, &sigchld_action, NULL);

    printf("SolarPost starts---\n");

    memset(buf,'\0',sizeof(buf));

#ifndef DEBUG

    snprintf(buf, 511, "%s;argc is %d, argv 1 %s", LOG, argc, argv[1]);

    msgType = atoi(argv[1]);

    sleep(10);

#else

    printf(YELLOW"SolarPost is in debug mode\n"NONE);

#endif

    act.sa_handler = (__sighandler_t)ShutdownSystemBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    //Ignore SIGINT
    
    signal(SIGINT, SIG_IGN);

    ret = GetConfig();

    if(ret == -1)
    {
        printf("GetConfig return -1\n");
        exit(0);
    }

#ifndef DEBUG  //Dealing with real data

    

    msgId = sgsCreateMsgQueue(UPLOAD_AGENT_KEY, 0);
    if(msgId == -1)
    {
        printf("Open Upload Agent queue failed...\n");
        exit(0);
    }



    //Attach buffer pool

    ret = sgsInitBufferPool(0);

    //Get data info

    ret = sgsGetDataInfoFromBufferPool("SolarCollector", &(bufferInfo[0]));
    if(ret == -1)
    {

        printf("Failed to get data buffer info, return %d\n", ret);
        exit(0);

    }

    printf("Number of data is %d \n",bufferInfo[0].numberOfData);

    //Attach the dataInfo

    ret = sgsInitDataInfo(NULL, &(dInfo[0]), 0, "./conf/Collect/SolarCollector", bufferInfo[0].shmId, &numberOfData[0]);
    if(ret == -1)
    {

        printf("Attach shmid %d Failed\n",bufferInfo[0].shmId);
        exit(0);

    }
    else
    {
        if(dInfo[0] != NULL)
        printf("dInfo[0] is not NULL\n");
        else
        {
            printf("dInfo[0] is NULL!\n");
            exit(0);
        }


    }

    //get first timestamp

    time(&last);
    now = last;

    //main loop

    while(1) 
    {

        usleep(100000);
        time(&now);

        //check time interval

        if((now-last) >= interval + 4 )
        {

            //Update data
            //printf("generate new data\n");
            ret = PostToServer();
            
            if(ret == -3)
            {
                sgsSendQueueMsg(eventHandlerId, errResult, EnumUploadAgent);
            }

            printf("[%s:%d]Post return %d\n", __FUNCTION__, __LINE__, ret);


            //printf("show data\n");
            //sgsShowDataInfo(dInfo);
            //printf("got new time\n");
            time(&last);
            now = last;
            last -= 4;

        }

        //Check message

        ret = CheckAndRespondQueueMessage();

    }

#else //Debug build, providing test functions for message and revising configs

    eventHandlerId = sgsCreateMsgQueue(EVENT_HANDLER_KEY, 0);
    if(eventHandlerId == -1)
    {
        printf("Open eventHandler queue failed...\n");
        exit(0);
    }

    //Attach buffer pool

    ret = sgsInitBufferPool(0);
    
    //Get data info

    ret = sgsGetDataInfoFromBufferPool("SolarCollector", &(bufferInfo[0]));
    if(ret == -1)
    {

        printf("Failed to get data buffer info, return %d\n", ret);
        exit(0);

    }

    printf("Number of data is %d \n",bufferInfo[0].numberOfData);

    //Attach the dataInfo

    ret = sgsInitDataInfo(NULL, &(dInfo[0]), 0, "./conf/Collect/SolarCollector", bufferInfo[0].shmId, &numberOfData[0]);
    if(ret == -1)
    {

        printf("Attach shmid %d Failed\n",bufferInfo[0].shmId);
        exit(0);

    }

    //get first timestamp

    time(&last);
    now = last;

    printf("Old now %ld\n", now);

    struct tm *timeStruct;

    timeStruct = localtime(&now);

    now = mktime(timeStruct);

    printf("now %ld\n", now);

    time_t t = time(NULL);
    struct tm lt = {0};

    localtime_r(&t, &lt);
    
    printf("Offset to GMT is %lds.\n", lt.tm_gmtoff);
    printf("The time zone is '%s'.\n", lt.tm_zone);


    //main loop

    if(1) 
    {

        //check time interval

        if(1)
        {

            //Update data
#if 0
            ret = PostToServer();

            if(ret == -3)
            {
                sgsSendQueueMsg(eventHandlerId, errResult, EnumUploadAgent);
            }

            //sgsSendQueueMsg(eventHandlerId, "[Control];SGSlogger;SolarCollector;SaveLogNow", EnumLogger);
#else
            
            fp = fopen("./simPostSetting", "r");
            char buf[1024] = {0};
            fgets(buf, 1023, fp);
            printf("buf is \n%s\n",buf);
            SetConfig(buf, "203.73.24.133:9000/solar_rawdata");
#endif

        }

        //Check message

        ret = CheckAndRespondQueueMessage();

    }

    sgsDeleteDataInfo(dInfo[0], -1);

#endif

    return 0;

}

int GetConfig()
{

    FILE *fp = NULL;
    char buf[128];
    char *name;
    char *value;
    int i = 0;

    memset(&(postConfig),0,sizeof(postConfig));

    snprintf(postConfig.GW_ver, 15, "Alpha Build V1.0");

    snprintf(postConfig.IP[i], 127, "140.118.70.136:9010/solar_rawdata");
    
    postConfig.Send_Rate = 30;

    postConfig.Gain_Rate = 30;

    postConfig.Backup_time = 60;

    snprintf(postConfig.MAC_Address, 31, "7c:b0:c2:4f:76:1c");
    snprintf(postConfig.Station_ID, 15, "T910142");
    snprintf(postConfig.GW_ID, 15, "01");

    fp = fopen("./conf/Upload/SolarPostOldForm", "r");

    if(fp == NULL)
    {

        printf("SolarPost can't open the setting file. I'll use the default value\n");
        return 0;

    }
    else
    {

        printf("Hello\n");
        while(!feof(fp))
        {

            fgets(buf, 127, fp);

            name = strtok(buf, ";");
            printf("name is %s\n",name);
            value = strtok(NULL, ";");
            printf("value is %s\n", value);
            if(name != NULL)
            {
                if(!strcmp(name, "GW_ver"))
                {
    
                    snprintf(postConfig.GW_ver, 15, "%s", value);
    
                }
                else if(!strcmp(name, "IP_1"))
                {
    
                    snprintf(postConfig.IP[0], 127, "%s", value);
    
                }
                else if(!strcmp(name, "IP_2"))
                {
    
                    snprintf(postConfig.IP[1], 127, "%s", value);
    
                }
                else if(!strcmp(name, "IP_3"))
                {
    
                    snprintf(postConfig.IP[2], 127, "%s", value);
    
                }
                else if(!strcmp(name, "IP_4"))
                {
    
                    snprintf(postConfig.IP[3], 127, "%s", value);
    
                }
                else if(!strcmp(name, "IP_5"))
                {
    
                    snprintf(postConfig.IP[4], 127, "%s", value);
    
                }
                else if(!strcmp(name, "Send_Rate"))
                {
    
                    postConfig.Send_Rate = atof(value) * 60;
    
                }
                else if(!strcmp(name, "Gain_Rate"))
                {
    
                    postConfig.Gain_Rate = atof(value) * 60;
    
                }
                else if(!strcmp(name, "Backup_time"))
                {
    
                    postConfig.Backup_time = atof(value);
    
                }
                else if(!strcmp(name, "MAC_Address"))
                {
    
                    snprintf(postConfig.MAC_Address, 31, "%s", value);
    
                }
                else if(!strcmp(name, "Station_ID"))
                {
    
                    snprintf(postConfig.Station_ID, 15, "%s", value);
    
                }
                else if(!strcmp(name, "GW_ID"))
                {
    
                    snprintf(postConfig.GW_ID, 15, "%s", value);
    
                }

            }

        }
        printf("Leave while\n");

    }   

    fclose(fp);

    for(i=0;i<5;i++)
        printf("postConfig.IP[%d] = %s\n", i, postConfig.IP[i]);

    
    return 0;

}

int SetConfig(char *result, char *address)
{

    cJSON *root;
    cJSON *temp;
    cJSON *flag;
    char buf[1200];
    char Resend_time_s[32];
    char Resend_time_e[32];
    FILE *fp;
    int i = 0, resend = 0, ret = -1;
    pid_t pid;

    root = cJSON_Parse(result);

    if(root == NULL)
    {

        printf("%s is not a JSON\n", result);
        return 0;
        
    }

    temp = cJSON_GetObjectItem(root, "Upload_data");

    if(temp != NULL && temp->type == 1)
    {


        return -1; //resend flag is on

    }

    flag = cJSON_GetObjectItem(root, "Config_flag");

    if(flag != NULL && temp->type == 2)
    {

        fp = fopen("./conf/Upload/SolarPostOldForm_tmp", "w");

        if(fp == NULL)
        {

            printf("Open conf file failed, discard the changes\n");
            return -2;

        }
        else //Parse command and issue queue message
        {

            for(i = 1 ; i <= 17 ; i++)
            {

                printf("Loop i = %d\n", i);
                switch(i)
                {

                    case 1:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "GW_ver");
                    break;

                    case 2:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "IP_1");
                    break;

                    case 3:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "IP_2");
                    break;

                    case 4:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "IP_3");
                    break;

                    case 5:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "IP_4");
                    break;

                    case 6:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "IP_5");
                    break;

                    case 7:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "DateTime");
                    break;

                    case 8:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "Send_rate");
                    break;

                    case 9:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "Gain_rate");
                    break;

                    case 10:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "Resend");
                    break;

                    case 11:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "Backup_time");
                    break;

                    case 12:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "MAC_Address");
                    break;

                    case 13:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "Station_ID");
                    break;

                    case 14:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "GW_ID");
                    break;

                    case 15:
                    if(!resend) break;
                    memset(buf,0,sizeof(buf));
                    memset(Resend_time_s,0,sizeof(Resend_time_s));
                    snprintf(buf, sizeof(buf) -1, "Resend_time_s");
                    printf("time_s type %d\n", temp->type);
                    temp = cJSON_GetObjectItem(root, buf);
                    if(temp != NULL ) snprintf(Resend_time_s, sizeof(Resend_time_s) - 1, "%s", temp->valuestring );
                    break;

                    case 16:
                    if(!resend) break;
                    memset(buf,0,sizeof(buf));
                    memset(Resend_time_e,0,sizeof(Resend_time_e));
                    snprintf(buf, sizeof(buf) -1, "Resend_time_e");
                    printf("time_e type %d\n", temp->type);
                    temp = cJSON_GetObjectItem(root, buf);
                    if(temp != NULL ) snprintf(Resend_time_e, sizeof(Resend_time_e) - 1, "%s", temp->valuestring );
                    break;

                    case 17:
                    memset(buf,0,sizeof(buf));
                    snprintf(buf, sizeof(buf) -1, "Command");
                    break;


                }

                if(i < 15)
                {

                    temp = cJSON_GetObjectItem(root, buf);

                    if(i == 10 && temp != NULL )
                    {

                        printf("resend type is %d\n", temp->type);  
                        if(temp->type == 2);
                        {
                            resend = 1;
                        }
                        
                    }
                    else
                    {
                        if(temp != NULL )
                        {

                            if(temp->valuestring != NULL)
                                fprintf(fp, "%s;%s\n", temp->string, temp->valuestring);

                        }
                        else
                        {

                            memset(buf, 0, sizeof(buf));
                            snprintf(buf, sizeof(buf) - 1, "%s;Config format incorrect: %s\n", ERROR, result);

                            sgsSendQueueMsg(eventHandlerId, buf, msgType);

                            fclose(fp);
                            return -2;
                        
                        }
                    }
                }
                
            }
            
        }

    }
    cJSON_Delete(root);
    ret = rename("./conf/Upload/SolarPost_tmp", "./conf/Upload/SolarPostOldForm");
    if(ret != 0)
    {

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "%s;Rename SolarPostOldForm file failed\n", ERROR);
        sgsSendQueueMsg(eventHandlerId, buf, msgType);

    }
    printf("resend = %d, time_s = %s, time_e = %s\n", resend, Resend_time_s, Resend_time_e);
    fclose(fp);
    if(resend)
    {

        pid = fork();
        if(pid == 0)
        {
            execlp("./SolarPut","./SolarPut", Resend_time_s, Resend_time_e, address,NULL);
            perror("execlp");
            exit(0);
        }

    }

}

int PostToServer()
{

    dataInfo *tempInfo[2] = {dInfo[0], dInfo[0]};
    dataLog dLog;
    epochTime nowTime;
    char buf[256];
    char *jsonOutput = NULL;
    char *jsonBuff = NULL;
    cJSON *root = NULL;
    cJSON *rows = NULL;
    cJSON *obj = NULL;
    cJSON *inverter = NULL;
    cJSON *tempArray = NULL;
    cJSON *irrArray = NULL;
    cJSON *alarmArray = NULL;
    cJSON *duplicatTempArray = NULL;
    cJSON *duplicateIrrArray = NULL;
    cJSON *tempObj = NULL;
    int i = 0, count = 10, ret = -1, inverterID = 1;
    int PVTemp = 0;
    typedef struct sysNode
    {

        int cpuUsage;
        int memoryUsage;
        int storageUsage;
        int networkFlow;

    }sNode;

    sNode systemInfo;

    typedef struct irrNode
    {

        int irr;
        int status;

    }iNode;

    iNode irrInfo;

#if 1

    //The best steps to match the format is 
    //1. get system info
    //2. get solar info & form JSON at the same time
    //3. Post them
    //4. Process the return value

    systemInfo.cpuUsage = 0;
    systemInfo.memoryUsage = 0;
    systemInfo.storageUsage = 0;
    systemInfo.networkFlow = 0;

    //1. get system info

    if(tempInfo[1] == NULL)
    {

        printf(LIGHT_RED"Encounter some problem while accessing GWInfo's data buffer\n"NONE);
        memset(buf, 0, sizeof(buf));
        snprintf(buf, 255, "%s;Encounter some problem while accessing GWInfo's data buffer", ERROR);
        sgsSendQueueMsg(eventHandlerId, buf, msgId);
        return -1;

    }

    //2. get solar info & form JSON at the same time

    i = 0;

    //Second, Get inverter info & form JSON

    //Init JSON

    root = cJSON_CreateObject();

    rows = cJSON_CreateArray();

    //Add Field ID to root

    cJSON_AddItemToObject(root, "Station_ID",cJSON_CreateString("T0510604") );

    //create rows, inverter obj and do the first time insert

    cJSON_AddItemToObject(root, "rows", rows);

    inverter = cJSON_CreateObject();

    cJSON_AddItemToArray(rows, inverter);

    time(&nowTime);

    //Insert data to first inverter object

    cJSON_AddNumberToObject(inverter, "upload_timestamp", nowTime);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf) - 1, "%02d", inverterID);
    cJSON_AddStringToObject(inverter, "InverterID", buf);//insert inverterID (count manually by inverterID)

    while(tempInfo[0] != NULL)
    {

        sgsReadSharedMemory(tempInfo[0], &dLog);

        //If we meet Alarm, put PV_Temp and Irr into inverter obj then create a new object

        if(strstr(tempInfo[0]->valueName, "Inverter_Status"))
        {

            //Add system infos

            cJSON_AddNumberToObject(inverter, "Network_flow", systemInfo.networkFlow);
            cJSON_AddNumberToObject(inverter, "Memory", systemInfo.memoryUsage);
            cJSON_AddNumberToObject(inverter, "Storage", systemInfo.storageUsage);
            cJSON_AddNumberToObject(inverter, "CPU", systemInfo.cpuUsage);

            //Add Irr and PVTemp

            cJSON_AddNumberToObject(inverter, "Irr", irrInfo.irr);
            cJSON_AddNumberToObject(inverter, "PVTemp", PVTemp);

            //Add Inverter status

            cJSON_AddNumberToObject(inverter, "Inverter_Status", dLog.value.i);

            //Create New inverter obj

            if(tempInfo[0]->next != NULL)
            {
                
                inverter = cJSON_CreateObject();
                cJSON_AddItemToArray(rows, inverter);

                cJSON_AddNumberToObject(inverter, "upload_timestamp", nowTime);
                memset(buf, 0, sizeof(buf));

                //Next Inverter ID

                inverterID++;  
                snprintf(buf, sizeof(buf) - 1, "%02d", inverterID);

                //insert inverterID (count manually by inverterID)

                cJSON_AddStringToObject(inverter, "InverterID", buf);

            }
            

        }
        else    //keep filling in current object
        {


            if(strstr(tempInfo[0]->valueName, "CPU_Usage"))
            {

                systemInfo.cpuUsage = dLog.value.i;

            }
            else if(strstr(tempInfo[0]->valueName, "Storage_Usage"))
            {

                
                systemInfo.storageUsage = dLog.value.i;

            }
            else if(strstr(tempInfo[0]->valueName, "Memory_Usage"))
            {

                
                systemInfo.memoryUsage = dLog.value.i;

            }
            else if(strstr(tempInfo[0]->valueName, "Network_Flow"))
            {

                
                systemInfo.networkFlow = dLog.value.i;

            }
            else if(strstr(tempInfo[0]->valueName, "Irradiation")) // deal with IrrStatus together
            {

                irrInfo.irr = dLog.value.i;

                

            }
            else if(strstr(tempInfo[0]->valueName, "IrrStatus"))
            {

                //Since we are using IrrArray, this will be unused, but we'll keep it for a while
                irrInfo.status = dLog.value.i; 

            }
            else if(strstr(tempInfo[0]->valueName, "Temperature")) // same as Irr Array
            {

                PVTemp = dLog.value.i;

            }
            else if(strstr(tempInfo[0]->valueName, "Voltage(Vab)"))
            {

                cJSON_AddNumberToObject(inverter, "L1_ACV", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "CurrentA"))
            {

                cJSON_AddNumberToObject(inverter, "L1_ACA", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "WattageA"))
            {

                cJSON_AddNumberToObject(inverter, "L1_ACP", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "FrequencyA"))
            {

                cJSON_AddNumberToObject(inverter, "L1_AC_Freq", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "Voltage(Vbc)"))
            {

                cJSON_AddNumberToObject(inverter, "L2_ACV", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "CurrentB"))
            {

                cJSON_AddNumberToObject(inverter, "L2_ACA", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "WattageB"))
            {

                cJSON_AddNumberToObject(inverter, "L2_ACP", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "FrequencyB"))
            {

                cJSON_AddNumberToObject(inverter, "L2_AC_Freq", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "Voltage(Vca)"))
            {

                cJSON_AddNumberToObject(inverter, "L3_ACV", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "CurrentC"))
            {

                cJSON_AddNumberToObject(inverter, "L3_ACA", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "WattageC"))
            {

                cJSON_AddNumberToObject(inverter, "L3_ACP", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "FrequencyC"))
            {

                cJSON_AddNumberToObject(inverter, "L3_AC_Freq", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "VoltageDA"))
            {

                cJSON_AddNumberToObject(inverter, "DC1_DCV", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "CurrentDA"))
            {

                cJSON_AddNumberToObject(inverter, "DC1_DCA", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "WattageDA"))
            {

                cJSON_AddNumberToObject(inverter, "DC1_DCP", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "VoltageDB"))
            {

                cJSON_AddNumberToObject(inverter, "DC2_DCV", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "CurrentDB"))
            {

                cJSON_AddNumberToObject(inverter, "DC2_DCA", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "WattageDB"))
            {

                cJSON_AddNumberToObject(inverter, "DC2_DCP", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "Today_Wh"))
            {

                memset(buf,0,sizeof(buf));
                snprintf(buf, sizeof(buf) - 1, "%lld", dLog.value.ll);
                cJSON_AddNumberToObject(inverter, "ACP_Daily", dLog.value.ll);

            }
            else if(strstr(tempInfo[0]->valueName, "Life_Wh"))
            {

                memset(buf,0,sizeof(buf));
                snprintf(buf, sizeof(buf) - 1, "%lld", dLog.value.ll);
                cJSON_AddNumberToObject(inverter, "ACP_Life", dLog.value.ll);

            }
            else if(strstr(tempInfo[0]->valueName, "Inverter_Temp"))
            {

                cJSON_AddNumberToObject(inverter, "Inverter_Temp", dLog.value.i);

            }
            else if(strstr(tempInfo[0]->valueName, "Inverter_Error"))
            {

                cJSON_AddStringToObject(inverter,"Alarm_Log",dLog.value.s);

                cJSON_AddItemToObject(inverter,"Alarm", alarmArray = cJSON_CreateArray());

                cJSON_AddNumberToObject(alarmArray, "alarm_01", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_02", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_03", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_04", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_05", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_06", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_07", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_08", 0);
                cJSON_AddNumberToObject(alarmArray, "alarm_09", 0);

            }


        }

        tempInfo[0] = tempInfo[0]->next;

    }

    //3. Post them

    jsonBuff = cJSON_PrintUnformatted(root);

    printf("post body:\n%s\n",jsonBuff);

    i = 0;
    count = 10;

    while(i < 5)
    {


        if(strstr(postConfig.IP[i], "-"))
        {
            i++;
            continue;
        }
        printf(LIGHT_GREEN"postConfig.IP[%d] is %s\n"NONE, i, postConfig.IP[i]);

        ret = process_http(jsonBuff, postConfig.IP[i]);
        if(ret < 0 && count > 0)
        {
 
            count--;
            continue;

        }
        else if(ret >= 0)
        {

            i++;
            count = 10;

        }
        else if(count < 0)
        {

            memset(buf,0,sizeof(buf));
            snprintf(buf,sizeof(buf) -1, "%s;upload to %s failed",ERROR, postConfig.IP[i]);
            sgsSendQueueMsg(eventHandlerId, buf, msgId);
            count = 10;

        }
        

    }

    free(jsonBuff);
    cJSON_Delete(root);

    

#else

    while(tmpInfo != NULL)
    {
        
        sgsReadSharedMemory(tmpInfo, &dLog);

        memset(buf,'\0',sizeof(buf));

        strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &(dLog.updatedTime));

        if(!strcmp(tmpInfo->valueName,"AC1"))
        {

            printf("AC1: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"AC2"))
        {

            printf("AC2: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"AC3"))
        {

            printf("AC3: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"DC1"))
        {

            printf("DC1: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"DC2"))
        {

            printf("DC2: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"Daily"))
        {

            printf("Daily: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        else if(!strcmp(tmpInfo->valueName,"Error"))
        {

            printf("Error: %d\n",dLog.value.i);
            printf(YELLOW"updated time:%s\n\n"NONE,buf);

        }
        
        tmpInfo = tmpInfo->next;

    }

#endif

    return 0;

}

int CheckAndRespondQueueMessage()
{

    int ret = -1;
    char buf[MSGBUFFSIZE];
    char *cmdType = NULL;
    char *from = NULL;
    char *to = NULL;
    char *content = NULL;
    char result[MSGBUFFSIZE];

    memset(buf,'\0',sizeof(buf));

    ret = sgsRecvQueueMsg(msgId, buf, msgType);

    if(ret != -1)
    {

        cmdType = strtok(buf,SPLITTER);
        if(cmdType == NULL)
        {
            printf("Can't get the command type. the message is incomplete\n");
            return -1;
        }

        to = strtok(NULL,SPLITTER);
        if(to == NULL)
        {
            printf("Can't get the to. the message is incomplete\n");
            return -1;
        }

        from = strtok(NULL,SPLITTER);
        if(from == NULL)
        {
            printf("Can't get the from. the message is incomplete\n");
            return -1;
        }

        //return result

        memset(result,'\0',sizeof(result));

        snprintf(result,MSGBUFFSIZE - 1,"%s;%s;%s;command done.",RESULT,from,to);

        sgsSendQueueMsg(eventHandlerId, result, EnumCollector);

        return 0;
    
    }
    return 0;

}

int ShutdownSystemBySignal(int sigNum)
{

    printf("SolarPost bye bye\n");
    sgsDeleteDataInfo(dInfo[0], -1);
    sgsDeleteDataInfo(dInfo[1], -1);
    exit(0);

}

ssize_t process_http( char *content, char *address)
{
    
    int sockfd;
	struct sockaddr_in servaddr;
	char **pptr;
	char str[50];
	struct hostent *hptr;
	char sendline[MAXLINE + 1], recvline[MAXLINE + 1];
    int i = 0, ret = 0;
    char *error = NULL;
    char *hname = NULL;             //IP
    char *serverPort = NULL;        //port
    char page[128] = {'\0'};        //rest api
    char adrBuf[128] = {'\0'};
    char *tmp = NULL;
	ssize_t n;
    cJSON *root = NULL;
    cJSON *obj = NULL;

#ifdef DEBUG

    printf("process_http started\nAddress is %s\nContent is %s\n", address, content);

#endif

    //process address (host name, page, port) example, 203.73.24.133:8000/solar_rawdata

    strncpy(adrBuf, address, sizeof(adrBuf));

    hname = strtok(adrBuf, ":");
    serverPort = strtok(NULL, "/");
    tmp = strtok(NULL, "/");

    //Get what's behind the / 

    memset(page, 0, sizeof(page));
    snprintf(page, sizeof(page), "/%s",tmp);

    //Intialize host entity with server ip address

    if ((hptr = gethostbyname(hname)) == NULL) 
    {

		printf("[%s:%d] gethostbyname error for host: %s: %s", __FUNCTION__, __LINE__,hname ,hstrerror(h_errno));

		return -1;

	}

	printf("[%s:%d] hostname: %s\n",__FUNCTION__,__LINE__, hptr->h_name);

    //Set up address type (FAMILY)

	if (hptr->h_addrtype == AF_INET && (pptr = hptr->h_addr_list) != NULL) 
    {

		printf("[%s:%d] address: %s\n",__FUNCTION__,__LINE__,inet_ntop( hptr->h_addrtype , *pptr , str , sizeof(str) ));

	} 
    else
    {

		printf("[%s:%d] Error call inet_ntop \n",__FUNCTION__,__LINE__);

        return -1;

	}

    //Create socket

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd == -1)
    {

        printf("socket failed, errno %d, %s\n",errno,strerror(errno));
        return -1;

    }
    else
    {

        printf("[%s,%d]socket() done\n",__FUNCTION__,__LINE__);

    }

    //Set to 0  (Initialization)

	bzero(&servaddr, sizeof(servaddr));

    //Fill in the parameters

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(serverPort));
	inet_pton(AF_INET, str, &servaddr.sin_addr);

    //Connect to the target server

	ret = connect(sockfd, (SA *) & servaddr, sizeof(servaddr));

    if(ret == -1)
    {

        printf("connect() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    else
    {

        printf("[%s,%d]connect() done\n",__FUNCTION__,__LINE__);

    }

    //Header content for HTTP POST

	snprintf(sendline, MAXSUB,
		 "POST %s HTTP/1.1\r\n"
		 "Host: %s\r\n"
		 "Content-type: application/json; charset=UTF-8\r\n"
         "User-Agent: Kelier/0.1\r\n"
		 "Content-Length: %lu\r\n\r\n"
		 "%s", page, hname, strlen(content), content);

    //print out the content

    printf("sendline : \n %s\n",sendline);

    //Send the packet to the server

    ret = my_write(sockfd, sendline, strlen(sendline));

    if(ret == -1)
    {

        printf("write() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    printf("[%s,%d]Write() done\n",__FUNCTION__,__LINE__);

    //Get the result

    memset(recvline, 0, sizeof(recvline));

    ret = my_read(sockfd, recvline, (sizeof(recvline) - 1));

    if(ret == -1)
    {

        printf("read() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    printf("[%s,%d]Read done\n",__FUNCTION__,__LINE__);

    printf("return string:\n%s\n",recvline);

    tmp = strstr(recvline, "{");

    if(tmp == 0) 
    {

        memset(errResult, 0, sizeof(errResult));
        snprintf(errResult, sizeof(errResult), "%s;Return string:\n%s\n", ERROR, recvline);
        return -3;
    
    }
    
    //Check the result with cJSON

    root = cJSON_Parse(tmp);
    if(root == NULL)
    {
        
        memset(errResult, 0, sizeof(errResult));
        snprintf(errResult, sizeof(errResult), "%s;Parse to JSON failed:\n%s\n", ERROR, tmp);
        return -3;
            
    }

    tmp = cJSON_Print(root);

    printf("tmp : \n%s\n",tmp);

    free(tmp);

    obj = cJSON_GetObjectItem(root, "Upload_data");

    if( obj != NULL)
    {

        printf("Get Upload_data attribute %s type %d \n", obj->string, obj->type );

        if(obj->type == 1) //upload unsuccessfully
        {

            cJSON_Delete(root);
            return -2; //tell PostToServer() to resend the data
            
        }
        else
        {

            printf("Upload success\n");
            obj = cJSON_GetObjectItem(root, "Config_flag");
            if( obj != NULL)
            {

                printf("Get Config_flag attribute\n");
                if(obj->type == 2) //Set config
                {

                    printf("Setting new config\n");
                    SetConfig(tmp, address);
                    cJSON_Delete(root);
                    return 0;

                }

            }

        }
        cJSON_Delete(root);

    }
    
    //close the socket
    close(sockfd);

	return ret;

}

int my_write(int fd, void *buffer, int length)
{

    int bytes_left;
    int written_bytes;
    char *ptr;

    ptr=buffer;
    bytes_left=length;

    while(bytes_left>0)
    {
            
            //printf("Write loop\n");
            written_bytes=write(fd,ptr,bytes_left);
            //printf("Write %d bytes\n",written_bytes);

            if(written_bytes<=0)
            {       

                if(errno==EINTR)

                    written_bytes=0;

                else             

                    return(-1);

            }

            bytes_left-=written_bytes;
            ptr+=written_bytes;   

    }

    return(0);

}

int my_read(int fd, void *buffer, int length)
{

    int bytes_left;
    int bytes_read;
    char *ptr;
    
    ptr=buffer;
    bytes_left=length;

    while(bytes_left>0)
    {

        bytes_read=read(fd,ptr,bytes_read);

        if(bytes_read<0)
        {

            if(errno==EINTR)

                bytes_read=0;

            else

                return(-1);

        }

        else if(bytes_read==0)
            break;

        bytes_left-=bytes_read;
        ptr+=bytes_read;

    }

    return(length-bytes_left);

}
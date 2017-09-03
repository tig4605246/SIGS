/*

    Name: Xi-Ping Xu
    Date: July 19,2017
    Last Update: August 17,2017
    Program statement: 
        This is a agent that collect data from delta-rpi and irr meter 
        It has following functions:
        1. Register two data buffers to the data buffer pool (delta-rpi and irr)
            1. Init dataInfo
            2. Read in port pathes and open ports
            (Plan to use a struct array to store these two infos)
        2. Refresh data buffer every 30 seconds (including Inverter and irr meter)
            1.fetch data from modbus
            2.Update data buffer
        3. Receive, execute and return queue messages

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"
#include "../protocol/SGSmodbus.h"

#define SIMULATION

int RegisterDataBuffer(char *infoName, int sharedMemoryId, int numberOfInfo);

int FetchAndUpdateInfoTable();

int SimulateAndUpdateInfoTable();

int CheckAndRespondQueueMessage();

int ShutdownSystemBySignal(int sigNum);

int InitInfoTable(int *tagNum);

int OpenPorts();

typedef struct 
{

    char infoName[32];      //  The name of the infoTable
    char portPath[32];
    char portParam[32];
    int fd;                 //  it stores the file descriptor that represents port

}infoTable;

infoTable iTable[10]; //I suggest we use this to help us match the ports and data

dataInfo *dInfo;    // pointer to the shared memory
int interval = 10;  // time period between last and next collecting section
int eventHandlerId; // Message queue id for event-handler
int shmId;          // shared memory id
int msgId;          // created by sgsCreateMessageQueue
int msgType;        // 0 1 2 3 4 5, one of them

int main(int argc, char *argv[])
{

    int i = 0, ret = 0, numberOfData = 0, looping = 0;
    char buf[512];
    FILE *fp = NULL;
    time_t last, now;
    struct sigaction act, oldact;  

    printf("Solar Collector starts---\n");

    memset(buf,'\0',sizeof(buf));

    snprintf(buf,511,"%s;argc is %d, argv 1 %s", LOG, argc, argv[1]);

    msgType = atoi(argv[1]);

    act.sa_handler = (__sighandler_t)ShutdownSystemBySignal;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGTERM, &act, &oldact);

    //Ignore SIGINT
    
    signal(SIGINT, SIG_IGN);

    //Open message queue

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

    //Initialize infoTable

    ret = InitInfoTable(&numberOfData);
    if(ret == -1)
    {

        printf("Failed to initialize infoTable, return %d\n", ret);
        exit(0);

    }

    //Attach buffer pool

    ret = sgsInitBufferPool(0);

    //Registration

    ret = sgsRegisterDataInfoToBufferPool("ZKWZKSCollector", shmId, numberOfData);
    if(ret == -1)
    {

        printf("Failed to register, return %d\n", ret);
        sgsDeleteDataInfo(dInfo, shmId);
        exit(0);

    }

    //Open modbus ports

#ifndef SIMULATION

    ret = OpenPorts();
    if(ret == -1)
    {

        printf("Failed to Open ports, return %d\n", ret);
        sgsDeleteDataInfo(dInfo, shmId);
        exit(0);

    }

#endif

    //get first timestamp

    time(&last);
    now = last;

    //main loop

    while(1) 
    {

        usleep(100000);
        time(&now);

        //check time interval

        if( ((now-last) >= (interval +2) ))
        {

            //Update data

#ifdef SIMULATION

            printf("simulate new data\n");
            ret = SimulateAndUpdateInfoTable();

#else

            printf("generate new data\n");
            ret = FetchAndUpdateInfoTable();

#endif

            //printf("show data\n");
            //sgsShowDataInfo(dInfo);

            time(&last);
            now = last;
            last -= 2;

        }
        else
        {
            looping = 0;
        }

        //Check message

        ret = CheckAndRespondQueueMessage();

    }

}

int RegisterDataBuffer(char *infoName,int sharedMemoryId, int numberOfInfo)
{

    int ret = -1;

    ret = sgsRegisterDataInfoToBufferPool(infoName, sharedMemoryId, numberOfInfo);
    if(ret != 0)
    {

        printf("Registration failed, return %d\n", ret);
        printf("Delete dataInfo, shmid %d\n",ret);
        sgsDeleteDataInfo(dInfo, ret);
        exit(0);

    }
    return 0;

}

int FetchAndUpdateInfoTable()
{

    dataInfo *tmpInfo = NULL;
    dataLog dLog;
    int ret = -1, i = 0, j = 0, shift = 0, bitPos = 0;
    char buf[5] = {0}, codeName = 'E', stringBuf[128];
    unsigned char preCmd[8] = {0}, cmd[8] = {0}, res[64] = {0}, bit = 0x01;
    unsigned int crc = 0;
    long long resultValue = 0;

    
    //Loop all info tags

    tmpInfo = dInfo ;
    while(tmpInfo != NULL)
    {

        i = 0;
        while(strcmp("Wind",iTable[i].infoName))
            i++;

        memset(&dLog, 0, sizeof(dLog));
        memset(stringBuf, 0, sizeof(stringBuf));

        if(!strcmp(tmpInfo->deviceName, "ZKWZKS_Control"))
        {

            if(strstr(tmpInfo->valueName,"GMT") || strstr(tmpInfo->valueName, "8"))
            {

                dLog.valueType = STRING_VALUE;
                snprintf(dLog.value.s, sizeof(dLog.value.s) - 1, "Not writing %s", tmpInfo->valueName);
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else
            {

                dLog.valueType = STRING_VALUE;
                ret = sgsSendModbusCommandRTU(tmpInfo->modbusInfo.cmd, 8, 330000, tmpInfo->modbusInfo.response);

                //Put raw things in, because I don't know how to parse it

                for(j = 0 ; j < tmpInfo->modbusInfo.response[2] ; j++)
                {

                    snprintf(stringBuf, sizeof(stringBuf) -1, "%u ", tmpInfo->modbusInfo.response[j+3]);

                }
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }

        }
        else if(!strcmp(tmpInfo->valueName, "ZKWZKS_Info"))
        {

            
            ret = sgsSendModbusCommandRTU(tmpInfo->modbusInfo.cmd, 8, 330000, tmpInfo->modbusInfo.response);
            if(ret == -1 )
            {

                tmpInfo->modbusInfo.failCount += 1;

            }

            if(tmpInfo->modbusInfo.failCount > 10)
            {

                dLog.valueType = STRING_VALUE;
                snprintf(dLog.value.s, sizeof(dLog.value.s) - 1, "Read %s failed", tmpInfo->valueName);
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else
            {

                tmpInfo->modbusInfo.failCount = 0;
                if(strstr(tmpInfo->valueName, "TodayWatt"))
                {

                    dLog.valueType = LONGLONG_VALUE;
                    for(j = 0; j < 4 ; j++)
                    {

                        resultValue = tmpInfo->modbusInfo.response[j + 3];
                        dLog.value.ll  += resultValue * pow(256, 3 - j);
                        
                    }
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "TotalWatt"))
                {

                    dLog.valueType = LONGLONG_VALUE;
                    for(j = 0; j < 4 ; j++)
                    {

                        resultValue = tmpInfo->modbusInfo.response[j + 3];
                        dLog.value.ll  += resultValue * pow(256, 3 - j);
                        
                    }
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "SystemTime"))
                {

                    dLog.valueType = LONGLONG_VALUE;
                    for(j = 0; j < 4 ; j++)
                    {

                        resultValue = tmpInfo->modbusInfo.response[j + 3];
                        dLog.value.ll  += resultValue * pow(256, 3 - j);
                        
                    }
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "OutputLeakageCond"))
                {

                    dLog.valueType = INTEGER_VALUE;
                    dLog.value.i = tmpInfo->modbusInfo.response[3];
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "FanCond"))
                {

                    dLog.valueType = INTEGER_VALUE;
                    dLog.value.i = tmpInfo->modbusInfo.response[4];
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "LEDStat"))
                {

                    dLog.valueType = INTEGER_VALUE;
                    dLog.value.i = tmpInfo->modbusInfo.response[3];
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "GFStat"))
                {

                    dLog.valueType = INTEGER_VALUE;
                    dLog.value.i = tmpInfo->modbusInfo.response[4];
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "RT_min"))
                {

                    dLog.valueType = INTEGER_VALUE;
                    dLog.value.i = tmpInfo->modbusInfo.response[3];
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "RT_sec"))
                {

                    dLog.valueType = INTEGER_VALUE;
                    dLog.value.i = tmpInfo->modbusInfo.response[4];
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else if(strstr(tmpInfo->valueName, "JointCond"))
                {

                    dLog.valueType = INTEGER_VALUE;
                    dLog.value.i = tmpInfo->modbusInfo.response[3];
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }
                else
                {

                    dLog.valueType = LONGLONG_VALUE;
                    for(j = 0; j < 2 ; j++)
                    {

                        resultValue = tmpInfo->modbusInfo.response[j + 3];
                        dLog.value.ll  += resultValue * pow(256, 1 - j);
                        
                    }
                    sgsWriteSharedMemory(tmpInfo, &dLog);

                }

            }

        }

        
        tmpInfo = tmpInfo->next;

    }
    return 0;

}

int SimulateAndUpdateInfoTable()
{

    dataInfo *tmpInfo = NULL;
    dataLog dLog;
    int ret = -1, i = 0, fake;

    //Loop all info tags

    tmpInfo = dInfo ;
    while(tmpInfo != NULL)
    {

        memset(&dLog, 0, sizeof(dLog));

        fake = rand()%65536;

        //record Irr status

        snprintf(dLog.value.s, sizeof(dLog.value.s) -1, "%d", fake);
        
        dLog.valueType = STRING_VALUE;
        dLog.status = 1;
        sgsWriteSharedMemory(tmpInfo, &dLog);

        tmpInfo = tmpInfo->next;

    }
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

        sgsSendQueueMsg(eventHandlerId, result, msgId);

        return 0;
    
    }
    return 0;

}

int ShutdownSystemBySignal(int sigNum)
{

    printf("FakeTaida bye bye\n");
    sgsDeleteDataInfo(dInfo, shmId);
    exit(0);

}

int InitInfoTable(int *tagNum)
{

    int i = 0, ret = -1;
    FILE *fp = NULL;
    char buf[256] = {0};
    char *name = NULL;
    char *path = NULL;
    char *param = NULL;
    dataInfo *tmpInfo = NULL;
    unsigned short crc = 0;

    //Open port config and prepare iTable

    fp = fopen("./conf/Collect/ZKWZKSCollector_Port","r");
    if(fp == NULL)
    {

        printf(LIGHT_RED"Failed to open ./conf/Collect/ZKWZKSCollector_Port, bye bye.\n "NONE);
        exit(0);

    }

    //init struct array

    memset(&(iTable), 0, sizeof(iTable)); 

    i = 0;
    while( !feof(fp)) //fill up the struct array with port config
    {

        memset(buf,'\0',sizeof(buf));
        if(fscanf(fp, "%[^\n]\n", buf) < 0) 
            break;

        name = strtok(buf, ";");
        path = strtok(NULL,";");
        param = strtok(NULL, ";");
        strncpy(iTable[i].infoName, name, sizeof(iTable[i].infoName));
        strncpy(iTable[i].portPath, path, sizeof(iTable[i].portPath));
        strncpy(iTable[i].portParam, param, sizeof(iTable[i].portParam));
        i++;
        

    }

    for(i = 0 ; i < 2 ; i++)
    {
        
        if(iTable[i].infoName != 0)
            printf("iTable[%d].infoName %s, iTable[%d].portPath %s, iTable[%d].portParam %s\n", i, iTable[i].infoName, i, iTable[i].portPath, i, iTable[i].portParam);

    }

    for(i = 0 ; i < 1 ; i++)
    {

        ret = sgsInitDataInfo(NULL, &dInfo, 1, "./conf/Collect/ZKWZKSCollector", -1, tagNum);

        if(ret < 0 )
        {

            printf("failed to create dataInfo, ret is %d\n",ret);
            sgsSendQueueMsg(eventHandlerId,"[Error];failed to create dataInfo",9);
            exit(0);

        }

        tmpInfo = dInfo;

        while(tmpInfo != NULL)
        {

            if(!strcmp(tmpInfo->deviceName,"ZKWZKS_Control"))
            {

                if(strstr(tmpInfo->valueName, "16"))
                {

                    tmpInfo->modbusInfo.cmd[0x01] = 0x03;
                    
                    tmpInfo->modbusInfo.cmd[0x02] = (tmpInfo->modbusInfo.address & 0xff00) >> 8;
                    tmpInfo->modbusInfo.cmd[0x03] = (tmpInfo->modbusInfo.address & 0x00ff);
                    tmpInfo->modbusInfo.cmd[0x04] = (tmpInfo->modbusInfo.readLength & 0xff00) >> 8;
                    tmpInfo->modbusInfo.cmd[0x05] = (tmpInfo->modbusInfo.readLength & 0x00ff);

                }
                else if(strstr(tmpInfo->valueName, "K8"))
                {
                    
                    tmpInfo->modbusInfo.cmd[0x01] = 0x04; // Prevent accidentally change the value (correct command 0x17)
                    tmpInfo->modbusInfo.cmd[0x02] = (tmpInfo->modbusInfo.address & 0xff00) >> 8;
                    tmpInfo->modbusInfo.cmd[0x03] = (tmpInfo->modbusInfo.address & 0x00ff);
                    tmpInfo->modbusInfo.cmd[0x04] = (tmpInfo->modbusInfo.readLength & 0xff00) >> 8;
                    tmpInfo->modbusInfo.cmd[0x05] = (tmpInfo->modbusInfo.readLength & 0x00ff);

                }
                else if(strstr(tmpInfo->valueName, "GMT"))
                {

                    tmpInfo->modbusInfo.cmd[0x01] = 0x04; // Prevent accidentally change the value (correct command 0x17)
                    tmpInfo->modbusInfo.cmd[0x02] = (tmpInfo->modbusInfo.address & 0xff00) >> 8;
                    tmpInfo->modbusInfo.cmd[0x03] = (tmpInfo->modbusInfo.address & 0x00ff);
                    tmpInfo->modbusInfo.cmd[0x04] = (tmpInfo->modbusInfo.readLength & 0xff00) >> 8;
                    tmpInfo->modbusInfo.cmd[0x05] = (tmpInfo->modbusInfo.readLength & 0x00ff);

                }
                else if(strstr(tmpInfo->valueName, "DeviceMessage")) // Special command
                {

                    tmpInfo->modbusInfo.cmd[0x01] = 0x2b;
                    tmpInfo->modbusInfo.cmd[0x02] = 0x0e;
                    tmpInfo->modbusInfo.cmd[0x03] = 0x01;
                    tmpInfo->modbusInfo.cmd[0x04] = 0x00;

                }
                else
                {
                    tmpInfo->modbusInfo.cmd[0x01] = 0x04;
                }
                

            }
            else if(!strcmp(tmpInfo->deviceName,"ZKWZKS_Info")) //Typical Data request
            {

                tmpInfo->modbusInfo.cmd[0x01] = 0x04;
                
                tmpInfo->modbusInfo.cmd[0x02] = (tmpInfo->modbusInfo.address & 0xff00) >> 8;
                tmpInfo->modbusInfo.cmd[0x03] = (tmpInfo->modbusInfo.address & 0x00ff);
                tmpInfo->modbusInfo.cmd[0x04] = (tmpInfo->modbusInfo.readLength & 0xff00) >> 8;
                tmpInfo->modbusInfo.cmd[0x05] = (tmpInfo->modbusInfo.readLength & 0x00ff);   

            }

            tmpInfo->modbusInfo.cmd[0x00] = tmpInfo->modbusInfo.ID;
            crc = sgsCaculateCRC(tmpInfo->modbusInfo.cmd, 6);
            tmpInfo->modbusInfo.cmd[0x06] = (crc & 0xff00) >> 8;
            tmpInfo->modbusInfo.cmd[0x07] = crc & 0x00ff;

            tmpInfo = tmpInfo->next;

        }

        printf("Create info return %d, data number %d\n", ret, *tagNum);

        //Store shared memory id

        shmId = ret;

        //Show data

        //printf("Show dataInfo\n");
        

    }

    return 0;

}

int OpenPorts()
{

    int i = 0;

    for(i = 0 ; i < 2 ; i++)
    {

        iTable[i].fd =  sgsSetupModbusRTU(iTable[i].portPath, iTable[i].portParam);
        if(iTable[i].fd < 0)
        {

            perror("sgsSetupModbusRTU");
            return -1;

        }

    }
    return 0;

}
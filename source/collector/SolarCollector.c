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

    ret = sgsRegisterDataInfoToBufferPool("SolarCollector", shmId, numberOfData);
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
            sgsShowDataInfo(dInfo);

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
    char buf[5] = {0}, codeName = 'E';
    unsigned char preCmd[8] = {0}, cmd[8] = {0}, res[64] = {0}, bit = 0x01;
    unsigned int crc = 0;

    //Loop all info tags

    tmpInfo = dInfo ;
    while(tmpInfo != NULL)
    {

        i = 0;

        if(!strcmp(tmpInfo->deviceName,"Irr"))
        {

            while(strcmp("Irr",iTable[i].infoName))
                i++;

            ret = sgsSendModbusCommandRTU(tmpInfo->modbusInfo.cmd, 8, 330000, tmpInfo->modbusInfo.response);
            if(ret == -1)
            {

                printf(LIGHT_RED"[%s;%d]Failed to fetch %s's %s info \n"NONE, __FUNCTION__, __LINE__, tmpInfo->sensorName, tmpInfo->valueName);

            }

            memset(&dLog, 0, sizeof(dLog));
            dLog.value.i = tmpInfo->modbusInfo.response[3]*256 + tmpInfo->modbusInfo.response[4];
            dLog.valueType = INTEGER_VALUE;
            dLog.status = 1;
            sgsWriteSharedMemory(tmpInfo, &dLog);

        }
        else if(!strcmp(tmpInfo->deviceName,"Deltarpi"))
        {

            if(!strcmp("Voltage",tmpInfo->valueName) || !strcmp("Current",tmpInfo->valueName) || !strcmp("Wattage",tmpInfo->valueName) 
            || !strcmp("Frequency",tmpInfo->valueName))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;

                preCmd[0x00] = tmpInfo->modbusInfo.ID;
                preCmd[0x01] = 0x06;
                preCmd[0x02] = 0x03;
                preCmd[0x03] = 0x1f;
                preCmd[0x04] = 0x00;
                preCmd[0x05] = tmpInfo->modbusInfo.option;
                crc = sgsCaculateCRC(preCmd, 6);
                preCmd[0x06] = crc & 0xff00;
                preCmd[0x07] = crc & 0x00ff;

                //Set register value

                ret = sgsSendModbusCommandRTU(preCmd, 8, 330000, res);
                if(ret == -1)
                {

                    printf(LIGHT_RED"[%s;%d]Failed to control %s's %s info \n"NONE, __FUNCTION__, __LINE__, tmpInfo->sensorName, tmpInfo->valueName);
                    return -1;

                }

                //Get register value

                ret = sgsSendModbusCommandRTU(tmpInfo->modbusInfo.cmd, 8, 330000, tmpInfo->modbusInfo.response);
                if(ret == -1)
                {

                    printf(LIGHT_RED"[%s;%d]Failed to fetch %s's %s info \n"NONE, __FUNCTION__, __LINE__, tmpInfo->sensorName, tmpInfo->valueName);
                    return -1;

                }

                //Write back to shared memory

                memset(&dLog, 0, sizeof(dLog));
                dLog.value.i = tmpInfo->modbusInfo.response[3]*256 + tmpInfo->modbusInfo.response[4];
                dLog.valueType = INTEGER_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else if(!strcmp("Today_Wh",tmpInfo->valueName) || !strcmp("Life_Wh",tmpInfo->valueName))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;

                //Get register value

                ret = sgsSendModbusCommandRTU(tmpInfo->modbusInfo.cmd, 8, 330000, tmpInfo->modbusInfo.response);
                if(ret == -1)
                {

                    printf(LIGHT_RED"[%s;%d]Failed to fetch %s's %s info \n"NONE, __FUNCTION__, __LINE__, tmpInfo->sensorName, tmpInfo->valueName);
                    return -1;

                }

                //Write back to shared memory

                memset(&dLog, 0, sizeof(dLog));
                dLog.value.ll = tmpInfo->modbusInfo.response[3]*256*256*256 ;
                dLog.value.ll += tmpInfo->modbusInfo.response[4]*256*256 + tmpInfo->modbusInfo.response[5]*256 + tmpInfo->modbusInfo.response[6];
                dLog.valueType = LONGLONG_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else if(!strcmp("Inverter_Temp",tmpInfo->valueName))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;

                //Get register value

                ret = sgsSendModbusCommandRTU(tmpInfo->modbusInfo.cmd, 8, 330000, tmpInfo->modbusInfo.response);
                if(ret == -1)
                {

                    printf(LIGHT_RED"[%s;%d]Failed to fetch %s's %s info \n"NONE, __FUNCTION__, __LINE__, tmpInfo->sensorName, tmpInfo->valueName);
                    return -1;

                }

                //Write back to shared memory

                memset(&dLog, 0, sizeof(dLog));
                dLog.value.i = tmpInfo->modbusInfo.response[3]*256 + tmpInfo->modbusInfo.response[4];
                dLog.valueType = INTEGER_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else if(!strcmp("Inverter_Error",tmpInfo->valueName))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;

                memset(&dLog, 0, sizeof(dLog));

                preCmd[0x00] = tmpInfo->modbusInfo.ID;
                preCmd[0x01] = 0x04;

                preCmd[0x04] = 0x00;
                preCmd[0x05] = 0x01;
                

                for(j = 0 ; j < 10 ; j++)
                {

                    shift = 0;
                    switch(j)
                    {

                        case 0:
                            codeName = 'E';
                            preCmd[0x02] = 0x0b;
                            preCmd[0x03] = 0xff;
                        break;

                        case 1:
                            preCmd[0x02] = 0x0a;
                            preCmd[0x03] = 0x00;
                            shift = 16;
                        break;

                        case 2:
                            preCmd[0x02] = 0x0a;
                            preCmd[0x03] = 0x01;
                            shift = 32;
                        break;

                        case 3:
                            codeName = 'W';
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x0f;
                        break;

                        case 4:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x10;
                            shift = 16;
                        break;

                        case 5:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x11;
                            shift = 32;
                        break;

                        case 6:
                            codeName = 'F';
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x1f;
                        break;

                        case 7:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x20;
                            shift = 16;
                        break;

                        case 8:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x21;
                            shift = 32;
                        break;

                        case 9:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x22;
                            shift = 48;
                        break;

                        default:

                        break;



                    }

                    crc = sgsCaculateCRC(preCmd, 6);
                    preCmd[0x06] = crc & 0xff00;
                    preCmd[0x07] = crc & 0x00ff;

                    //Execute command 

                    ret = sgsSendModbusCommandRTU(preCmd, 8, 330000, tmpInfo->modbusInfo.response);
                    if(ret == -1)
                    {

                        printf(LIGHT_RED"[%s;%d]Failed to fetch %s's %s info \n"NONE, __FUNCTION__, __LINE__, tmpInfo->sensorName, tmpInfo->valueName);
                        return -1;

                    }

                    //Parse result

                    for(bitPos = 0 ; bitPos < 16 ; bitPos++)
                    {

                        if(bitPos == 8)
                            bit = 0x01;

                        if(bitPos < 8)
                        {
                            ret = tmpInfo->modbusInfo.response[4] & bit;
                            bit = bit << 1;
                            
                        }
                        else
                        {

                            ret = tmpInfo->modbusInfo.response[3] & bit;
                            bit = bit << 1;

                        }
                        if(ret > 0)
                        {

                            snprintf(buf, 4, "%c%02d", codeName, (bitPos + shift));
                            strcat(dLog.value.s,buf);

                        }

                    }

                }

                //Write back to shared memory
                dLog.valueType = STRING_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

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
    int ret = -1, i = 0, j = 0, shift = 0, bitPos = 0;
    char buf[5] = {0}, codeName = 'E';
    char *namePart = NULL;
    unsigned char preCmd[8] = {0}, cmd[8] = {0}, res[64] = {0}, bit = 0x01;
    unsigned int crc = 0;

    //Loop all info tags

    tmpInfo = dInfo ;
    while(tmpInfo != NULL)
    {

        namePart = strtok(tmpInfo->valueName, "-");
        i = 0;
        if(!strcmp(tmpInfo->deviceName,"Irr"))
        {

            while(strcmp("Irr",iTable[i].infoName))
                i++;

            memset(&dLog, 0, sizeof(dLog));
            dLog.value.i = rand()%256*256 + rand()%256;
            dLog.valueType = INTEGER_VALUE;
            dLog.status = 1;
            sgsWriteSharedMemory(tmpInfo, &dLog);

        }
        else if(!strcmp(tmpInfo->deviceName,"Deltarpi"))
        {

            if(strstr(namePart, "Voltage") || strstr(namePart, "Current") || strstr(namePart, "Wattage") 
            || strstr(namePart, "Frequency") || strstr(namePart, "Voltage(Vab)") || strstr(namePart, "Voltage(Vbc)")
            || strstr(namePart, "Voltage(Vca)"))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;
                preCmd[0x00] = tmpInfo->modbusInfo.ID;
                preCmd[0x01] = 0x06;
                preCmd[0x02] = 0x03;
                preCmd[0x03] = 0x1f;
                preCmd[0x04] = 0x00;
                preCmd[0x05] = tmpInfo->modbusInfo.option;
                crc = sgsCaculateCRC(preCmd, 6);
                preCmd[0x06] = crc & 0xff00;
                preCmd[0x07] = crc & 0x00ff;

                //Set register value

                //Get register value

                //Write back to shared memory

                memset(&dLog, 0, sizeof(dLog));
                dLog.value.i = rand()%256*256 + rand()%256;;
                dLog.valueType = INTEGER_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else if(!strcmp("Today_Wh",namePart) || !strcmp("Life_Wh",namePart))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;

                //Get register value

                //Write back to shared memory

                memset(&dLog, 0, sizeof(dLog));
                dLog.value.ll += (rand()%256 );
                dLog.value.ll = dLog.value.ll*256*256*256;
                dLog.value.ll += rand()%256*256*256 + rand()%256*256 + rand()%256;
                dLog.valueType = LONGLONG_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else if(!strcmp("Inverter_Temp",namePart))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;

                //Get register value

                //Write back to shared memory

                memset(&dLog, 0, sizeof(dLog));
                dLog.value.i = rand()%256*256 + rand()%256;
                dLog.valueType = INTEGER_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }
            else if(!strcmp("Inverter_Error",namePart))
            {

                while(strcmp("Deltarpi",iTable[i].infoName))
                    i++;

                memset(&dLog, 0, sizeof(dLog));

                preCmd[0x00] = tmpInfo->modbusInfo.ID;
                preCmd[0x01] = 0x04;

                preCmd[0x04] = 0x00;
                preCmd[0x05] = 0x01;
                
                shift = 0;

                for(j = 0 ; j < 10 ; j++)
                {

                    switch(j)
                    {

                        case 0:
                            codeName = 'E';
                            preCmd[0x02] = 0x0b;
                            preCmd[0x03] = 0xff;
                        break;

                        case 1:
                            preCmd[0x02] = 0x0a;
                            preCmd[0x03] = 0x00;
                            shift = 16;
                        break;

                        case 2:
                            preCmd[0x02] = 0x0a;
                            preCmd[0x03] = 0x01;
                            shift = 32;
                        break;

                        case 3:
                            codeName = 'W';
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x0f;
                        break;

                        case 4:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x10;
                            shift = 16;
                        break;

                        case 5:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x11;
                            shift = 32;
                        break;

                        case 6:
                            codeName = 'F';
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x1f;
                        break;

                        case 7:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x20;
                            shift = 16;
                        break;

                        case 8:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x21;
                            shift = 32;
                        break;

                        case 9:
                            preCmd[0x02] = 0x0c;
                            preCmd[0x03] = 0x22;
                            shift = 48;
                        break;

                        default:

                        break;



                    }

                    crc = sgsCaculateCRC(preCmd, 6);
                    preCmd[0x06] = crc & 0xff00;
                    preCmd[0x07] = crc & 0x00ff;

                    //Execute command 

                    //Parse result

                    for(bitPos = 0 ; bitPos < 16 ; bitPos++)
                    {

                        ret = -1;
                        if(bitPos == 8)
                            bit = 0x01;

                        if(bitPos < 8)
                        {
                            ret = rand()%2;
                            bit = bit << 1;
                            
                        }
                        else
                        {

                            ret = rand()%2;
                            bit = bit << 1;

                        }
                        if(ret > 0)
                        {

                            snprintf(buf, 4, "%c%02d", codeName, (bitPos + shift));
                            strcat(dLog.value.s,buf);

                        }

                    }

                }

                //Write back to shared memory
                dLog.valueType = STRING_VALUE;
                dLog.status = 1;
                sgsWriteSharedMemory(tmpInfo, &dLog);

            }

        }

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

    fp = fopen("./conf/Collect/SolarCollector_Port","r");
    if(fp == NULL)
    {

        printf(LIGHT_RED"Failed to open ./conf/Collect/SolarCollector_Port, bye bye.\n "NONE);
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

        printf("iTable[%d].infoName %s, iTable[%d].portPath %s, iTable[%d].portParam %s\n", i, iTable[i].infoName, i, iTable[i].portPath, i, iTable[i].portParam);

    }

    for(i = 0 ; i < 1 ; i++)
    {

        ret = sgsInitDataInfo(NULL, &dInfo, 1, "./conf/Collect/SolarCollector", -1, tagNum);

        if(ret < 0 )
        {

            printf("failed to create dataInfo, ret is %d\n",ret);
            sgsSendQueueMsg(eventHandlerId,"[Error];failed to create dataInfo",9);
            exit(0);

        }

        tmpInfo = dInfo;

        while(tmpInfo != NULL)
        {

            if(!strcmp(tmpInfo->deviceName,"Irr"))
            {

                tmpInfo->modbusInfo.cmd[0x01] = 03;

            }
            else if(!strcmp(tmpInfo->deviceName,"Deltarpi"))
            {

                tmpInfo->modbusInfo.cmd[0x01] = 04;

            }

            tmpInfo->modbusInfo.cmd[0x00] = tmpInfo->modbusInfo.ID;
            
            tmpInfo->modbusInfo.cmd[0x02] = (tmpInfo->modbusInfo.address & 0xff00) >> 8;
            tmpInfo->modbusInfo.cmd[0x03] = (tmpInfo->modbusInfo.address & 0x00ff);
            tmpInfo->modbusInfo.cmd[0x04] = (tmpInfo->modbusInfo.readLength & 0xff00) >> 8;
            tmpInfo->modbusInfo.cmd[0x05] = (tmpInfo->modbusInfo.readLength & 0x00ff);
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
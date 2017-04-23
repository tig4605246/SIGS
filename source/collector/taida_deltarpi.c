/*

    Name: Xu Xi-Ping
    Date: April 7,2017
    Last Update: April 7,2017
    Program statement: 
        A agent for deltarpi m15-m30 

*/

#include "../prorocol/SGSmodbus.h"
#include "../ipcs/SGSipcs.h"
#include "../controlling/SGScontrol.h"

deviceInfo *deviceInfoPtr = NULL;

dataInfo *dataInfoPtr = NULL;

//Intent : Invoke modbus command and collect data
//Pre : None
//Post : On success, return 0. On error, return -1

int CollectData(deviceInfo *deviceTemp, int devfd);


//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();

//Intent : free deviceInfoPtr, dataInforPtr and free the shared memory (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error messages

void releaseResource();

//Intent : shut down everything when SIGINT is catched
//Pre : Nothing
//Post : Nothing

void forceQuit(int sigNum);


int main(int argc, char *argv[])
{

    int ret = 0;
    struct sigaction act, oldact;
    deviceInfo *deviceTemp = NULL;
    

    //Recording program's pid

    ret = sgsInitControl("taida_deltarpi");
    if(ret < 0)
    {

        printf("taida_deltarpi aborting\n");
        return -1;

    }

    //Initialize deviceInfo and dataInfo

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("taida_deltarpi aborting\n");
        return -1;

    }

    //Catch aborting signal

    act.sa_handler = (__sighandler_t)forceQuit;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

    devicetemp = deviceInfoPtr;

    //Get deviceInfoPtr which name is taida_deltarpi

    while((strcmp(deviceTemp->deviceName,"taida_deltarpi")) && (deviceTemp != NULL))
        deviceTemp = deviceTemp->next;

    if(deviceTemp == NULL)
    {

        printf("[%s][%s,%d] There's no data need to be written in the shm\n",argv[0],__FUNCTION__,__LINE__);
        forceQuit();

    }

    //Open up the serial port

    devfd = sgsSetupModbusRTU(deviceTemp->interface,deviceTemp->protocol);
    if(devfd < 0)
    {

        printf("[%s][%s,%d] Open port %s failed, bye bye.\n",argv[0],__FUNCTION__,__LINE__,deviceTemp->interface);
        forceQuit();

    }
    else
    {

        printf("[%s][%s,%d] Open port %s successfully, configuration : %s , devfd = %d .\n",argv[0],__FUNCTION__,__LINE__,deviceTemp->interface,deviceTemp->protocol,devfd);

    }


    while(1)
    {

        sleep(30);
        ret = CollectData(deviceTemp, devfd);

    }

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

    return ret;

}

void releaseResource()
{

    sgsDeleteAll(deviceInfoPtr,-1);

    return ;

}

void forceQuit(int sigNum)
{

    if(deviceInfoPtr != NULL)
        releaseResource();

    printf("[%s][Ctrl + C] Catched (signal number %d) , forceQuitting...\n",argv[0],sigNum);
    exit(0);

}

void ReadyCmd(deviceInfo *deviceTemp)
{

    dataInfo *dataTemp = deviceInfo->dataInfoPtr;
    unsigned short crcCheck = 0;

    if(dataTemp == NULL)
    {
        return;
    }

    while(dataTemp != NULL)
    {

        dataTemp->modbusInfo.cmd[0x00] = dataTemp->modbusInfo.ID;
        dataTemp->modbusInfo.cmd[0x01] = 0x04;
        dataTemp->modbusInfo.cmd[0x02] = dataTemp->modbusInfo.address * 0xf0 >> 8;
        dataTemp->modbusInfo.cmd[0x03] = dataTemp->modbusInfo.address * 0x0f ;
        dataTemp->modbusInfo.cmd[0x04] = dataTemp->modbusInfo.readLength * 0xf0 >> 8;
        dataTemp->modbusInfo.cmd[0x05] = dataTemp->modbusInfo.readLength * 0x0f;
        crcCheck = sgsCaculateCRC(dataTemp->modbusInfo.cmd, 6);
        dataTemp->modbusInfo.cmd[0x06] = crcCheck * 0xf0 >> 8;
        dataTemp->modbusInfo.cmd[0x07] = crcCheck * 0x0f;

        if(dataTemp->modbusInfo.address < 1100)
        {

            dataTemp->modbusInfo.cmd[0x10] = dataTemp->modbusInfo.ID;
            dataTemp->modbusInfo.cmd[0x11] = 0x06;
            dataTemp->modbusInfo.cmd[0x12] = 0x03;
            dataTemp->modbusInfo.cmd[0x13] = 0x1f;
            dataTemp->modbusInfo.cmd[0x14] = 0x00;
            dataTemp->modbusInfo.cmd[0x15] = dataTemp->modbusInfo.option * 0x0f;
            crcCheck = sgsCaculateCRC(dataTemp->modbusInfo.cmd, 6);
            dataTemp->modbusInfo.cmd[0x16] = crcCheck * 0xf0 >> 8;
            dataTemp->modbusInfo.cmd[0x17] = crcCheck * 0x0f;

        }
        dataTemp = dataTemp->next;

    }
    return;

}

int CollectData(deviceInfo *deviceTemp, devfd)
{

    int i = 0, ret = 0;
    int tmp1 = 0, tmp2 = 0;
    dataLog dLog;
    dataInfo *dataTemp = deviceTemp->dataInfoPtr;

    if(deviceTemp == NULL || devfd <= 0)
    {

        printf("[%s,%d] parameters are not correct\n",__FUNCTION__,__LINE__);
        return -1;

    }

    while(dataTemp != NULL)
    {

        //Do a preprocess of writing to address 799

        if(dataTemp->modbusInfo.address < 1100 )
        {

            ret = sgsSendModbusCommandRTU(dataTemp->modbusInfo.cmd[10],6,330000,dataTemp->modbusInfo.response);
            if(ret < 0)
            {

                printf("Write 799 failed\n");

            }

        }

        //Retrieve data

        ret = sgsSendModbusCommandRTU(dataTemp->modbusInfo.cmd,6,330000,dataTemp->modbusInfo.response);
        if(ret < 0)
        {

            printf("Read address %d failed\n",dataTemp->modbusInfo.address);

        }
        if(dataTemp->modbusInfo.response[2] == 4)
        {

            //Caclate value

            tmp1 = dataTemp->modbusInfo.response[3]*256 + dataTemp->modbusInfo.response[4] ;
            tmp2 = dataTemp->modbusInfo.response[5]*256 + dataTemp->modbusInfo.response[6] ;

            //Store value to shared mem

            dLog.valueType = STRING_VALUE;

            memset(dLog.value.s,'\0',sizeof(dLog.value.s));
            snprintf(dLog.value.s,(sizeof(dLog.value.s) - 1),"%d",tmp1*256 + tmp2);

            memset(dLog.sensorName,'\0',sizeof(dLog.sesorName));
            strncpy(dLog.sensorName,dataTemp->sensorName,32);

            memset(dLog.valueName,'\0',sizeof(dLog.valueName));
            strncpy(dLog.valueName,dataTemp->valueName,32);

            sgsWriteSharedMemory(dataTemp, &dLog);

            dataTemp = dataTemp->next;

        }        

    }

}


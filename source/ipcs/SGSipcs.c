/*

    Name: Xu Xi-Ping
    Date: March 3,2017
    Last Update: March 29,2017
    Program statement: 
        we write the detail of the function here 

    Update log: 

         2017/03/29 : Attach dataInfoPtr to deviceInfoPtr at sgsInitDataInfo

*/

#include "SGSipcs.h"
#include "../events/SGSEvent.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutexattr_t mutex_attr;
pthread_condattr_t cond_attr;

void sgsDeleteDataInfo(dataInfo *dataInfoPtr, int shmid)
{

    dataInfo *head = dataInfoPtr;
    dataInfo *last = NULL;

    while(head != NULL)
    {

        if(head->shareMemoryPtr != NULL)
            shmdt(head->shareMemoryPtr);

        last = head;
        head = head->next;
        free(last);

    }
    if(shmid >= 0)
    {

        if (shmctl(shmid, IPC_RMID, 0) == -1)
        {
            
            perror("shmctl");

        }

    }

    return;

}

void sgsDeleteDeviceInfo(deviceInfo *deviceInfoPtr)
{

    deviceInfo *head = deviceInfoPtr;
    deviceInfo *last = deviceInfoPtr;

    while(head != NULL)
    {

        last = head;
        head = head->next;
        free(last);

    }

    return;

}

void sgsDeleteAll(deviceInfo *deviceInfoPtr, int shmid)
{

    deviceInfo *head = deviceInfoPtr;
    deviceInfo *last = deviceInfoPtr;

    //free allocated memory

    while(head != NULL)
    {

        last = head;
        head = head->next;

        if(last->dataInfoPtr != NULL)
            sgsDeleteDataInfo(last->dataInfoPtr, -1);

        free(last);

    }
    printf("[%s,%d] delete complete...\n",__FUNCTION__,__LINE__);
    //Free shared memory

    if(shmid >= 0)
    {

        if (shmctl(shmid, IPC_RMID, 0) == -1)
        {

            perror("shmctl");

        }

    }

    return;

}


int sgsInitDeviceInfo(deviceInfo **deviceInfoPtr)
{

    FILE *deviceConfigFile = NULL;
    int line = 0;
    char buf[2048] = "";
    char *sp1 = NULL, *sp2 = NULL, *sp3 = NULL;
    deviceInfo *tempPtr = NULL;

    *deviceInfoPtr = NULL;

    deviceConfigFile = fopen(DEVICECONF,"r");
    if(deviceConfigFile == NULL)
    {

        printf(LIGHT_RED"[%s,%d] Can't find the %s\n"NONE,__FUNCTION__,__LINE__,DEVICECONF);
        return -1;

    }
    while(!feof(deviceConfigFile))
    {

        memset(buf,'\0',sizeof(buf));
        if(fscanf(deviceConfigFile, "%[^\n]\n", buf) < 0) break;
        printf("[%s,%d]fscanf in %s\n",__FUNCTION__,__LINE__,buf);
        if(!strlen(buf)) continue;
        if(buf[0] == '#')
        {

            printf("Skipping %s\n",buf);
            continue;

        } 
        line++;

        //split the config  buf is deviceName, sp1 is interface, sp2 is protocolConfig

        sp1 = strchr(buf, ',');
        sp2 = sp3 = NULL;
        if(sp1 != NULL) sp2 = strchr(sp1+1, ',');
        if(sp2 != NULL) sp3 = strchr(sp2+1, ',');
        if(sp1 == NULL || sp2 == NULL || sp3 == NULL)
        {

            printf(LIGHT_RED"[ERR] Invalid bus config in line %d in %s \n"NONE,line ,DEVICECONF);
            sgsDeleteDeviceInfo(*deviceInfoPtr);
            fclose(deviceConfigFile);
            return -1;

        }
        *sp1 = 0; sp1 += 1;
        *sp2 = 0; sp2 += 1;
        *sp3 = 0; sp3 += 1;


        //Put device data into deviceInfo

        tempPtr = (deviceInfo *)malloc(sizeof(deviceInfo));
        memset((void *)tempPtr, 0, sizeof(deviceInfo));
        tempPtr->dataInfoPtr = NULL;
        tempPtr->next = NULL;
        strncpy(tempPtr->deviceName, buf, strlen(buf)); 
        tempPtr->deviceName[127] = 0;
        strncpy(tempPtr->interface, sp1, strlen(sp1)); 
        tempPtr->interface[127] = 0;
        strncpy(tempPtr->protocolConfig, sp2, strlen(sp2)); 
        tempPtr->protocolConfig[127] = 0;
        strncpy(tempPtr->description, sp3, strlen(sp3)); 
        tempPtr->description[127] = 0;

        //Initialize pid in deviceInfoPtr, default we put in -1

        tempPtr->subProcessPid = -1;

        //put tempPtr into linked-list which head is deviceInfoPtr;

        if(*deviceInfoPtr == NULL)
        {

            printf("[%s,%d]deviceInfoPtr is NULL\n",__FUNCTION__,__LINE__);
            *deviceInfoPtr = tempPtr;
            (*deviceInfoPtr)->next == NULL;

        }
        else
        {

            printf("[%s,%d]deviceInfoPtr is not NULL\n",__FUNCTION__,__LINE__);
            tempPtr->next = *deviceInfoPtr;
            *deviceInfoPtr = tempPtr;
            

        }

    }

    return 0;

}

int sgsInitDataInfo(deviceInfo *deviceInfoPtr, dataInfo **dataInfoPtr, int CreateShm)
{
    int line = 0, zero = 0, j, shmid = 0, ret = 0;
    char buf[512], *sp1, *sp2;
    FILE *dataConfigFile = NULL;
    void *shm;
    dataInfo *dataInfoPtrHead = (*dataInfoPtr);
    dataInfo *dataInfoPtrTemp = NULL;

    dataInfo *dataInfoPtrTemp3 = NULL;
    deviceInfo *deviceInfoHead = deviceInfoPtr;

    shareMem *shareMemPtr = NULL;

    if((dataConfigFile=fopen(DATACONF, "r"))==NULL)
    {

        perror("sgsInitDataInfo");
        return -1;

    }

    while(!feof(dataConfigFile))
    {
        memset(buf,'\0',sizeof(buf));
        if(fscanf(dataConfigFile, "%[^\n]\n", buf) < 0) 
            break;
        line++;
        //printf("[LINE %d]: %s\n", i, buf);

        //Skip the empty line
        printf("[%s,%d]fscanf %s\n",__FUNCTION__,__LINE__,buf);
        if(!strlen(buf))
        { 
            printf("[%s,%d]buf == 0\n",__FUNCTION__,__LINE__);
            zero++; 
            continue;
            
        }

        //Skip commented line

        if(buf[0] == '#')
        { 
            printf("[%s,%d]line is commented\n",__FUNCTION__,__LINE__);
            zero++; 
            continue;
        
        }
        
        //allocate new dataInfo

        dataInfoPtrTemp = (dataInfo *)malloc(sizeof(dataInfo));
        memset((void *)dataInfoPtrTemp, 0, sizeof(dataInfo));
        dataInfoPtrTemp->next = NULL;

        //split the input string and put them into the dataInfo

        sp2 = buf;
        for(j=0; j<6; j++)
        {

            sp1 = sp2;
            if(j<6)
            {

                sp2 = strchr(sp1, ',');
                
                if(sp2 == NULL)
                {

                    printf(LIGHT_RED"[ERR] Invalid format in line %d in %s\n"NONE,
                               line, DATACONF);
                    sgsDeleteDataInfo(dataInfoPtrHead,-1);
                    fclose(dataConfigFile);
                    return -1;

                }
                *sp2 = 0;
                sp2++;

            }
            printf("[%s,%d]sp1 = %s sp2 = %s\n",__FUNCTION__,__LINE__,sp1,sp2);
            switch(j)
            {
                
                //'ndeviceName[32]' field in string char formart

                case 0: 
                    strncpy(dataInfoPtrTemp->deviceName, sp1, strlen(sp1)); dataInfoPtrTemp->deviceName[strlen(sp1)] = 0;
                    break;

                //'sensorName[32]' field in string char format

                case 1: 
                    strncpy(dataInfoPtrTemp->sensorName, sp1, strlen(sp1)); dataInfoPtrTemp->sensorName[strlen(sp1)] = 0;
                    break;

                //'valueName[32]' field in string char format

                case 2: 
                    strncpy(dataInfoPtrTemp->valueName, sp1, strlen(sp1)); dataInfoPtrTemp->valueName[strlen(sp1)] = 0;
                    break;

                //'id' field in integer format

                case 3: 
                    dataInfoPtrTemp->modbusInfo.ID = atoi(sp1);
                    break;
                
                //'func' field in integer format
                /*
                case 4: 
                    ctag->func = atoi(sp1);
                    break;
                */

                //'addr' field in integer format
                
                case 4: 
                    dataInfoPtrTemp->modbusInfo.address = atoi(sp1);
                    break;

                //'readLength' field in integer format

                case 5: 
                    dataInfoPtrTemp->modbusInfo.readLength = atoi(sp1);
                    break;

                //'option' field in integer format

                case 6:
                    dataInfoPtrTemp->modbusInfo.option = atoi(sp1);
                    break;

                //invalid field

                default: 
                    printf(LIGHT_RED"[ERR] The format of %s is wrong!\n"NONE,DATACONF);
                    sgsDeleteDataInfo(dataInfoPtrHead,-1);
                    fclose(dataConfigFile);
                    return -1;

            }

        }

        //I attach dataInfo to its belonged deviceInfo at here

        deviceInfoHead = deviceInfoPtr;
        if(deviceInfoHead != NULL)
        {

            while(deviceInfoHead != NULL)
            {

                if(strcmp(deviceInfoHead->deviceName,dataInfoPtrTemp->deviceName))
                    deviceInfoHead = deviceInfoHead->next;

                else break;


            }
            if(deviceInfoHead != NULL)
            {

                if(deviceInfoHead-> dataInfoPtr == NULL)
                {

                    deviceInfoHead-> dataInfoPtr = dataInfoPtrTemp;

                }
                else
                {

                    dataInfoPtrTemp3 =  deviceInfoHead-> dataInfoPtr;

                    //printf("POI\n");
                    while(dataInfoPtrTemp3->next != NULL)
                        dataInfoPtrTemp3 = dataInfoPtrTemp3->next;

                    //printf("hi\n");
                    dataInfoPtrTemp3->next = dataInfoPtrTemp;

                }

            }
            else
            {
                printf("No matched device, freeing data\n");
                free(dataInfoPtrTemp);
            }

        }
        //printf("  %d: name=%s, daemon=%s, bus=%s, id=%d, addr=%d, cmd=%s, rectime=%d\n", i, ctag->name, ctag->daemon, ctag->bus, ctag->id, ctag->addr, ctag->cmd, ctag->rectime_in_sec);
    }
    fclose(dataConfigFile);
    printf("[Debug] line is %d, zero is %d\n",line,zero);
    line -= (zero );
    printf("[Debug] line is %d, zero is %d\n",line,zero);
    if(CreateShm)
    {

        /*
         * Create the segment.
         */
        shmid = shmget(SGSKEY, sizeof(struct dataInfo)*line,
                          IPC_EXCL | IPC_CREAT | 0666);
        //shmid = shmget(SGSKEY, sizeof(struct conf_tag)*i, IPC_CREAT | 0666);

    }
    else
    {

        /*
         * Locate the segment.
         */
        shmid = shmget(SGSKEY, sizeof(struct dataInfo)*line, 0666);

    }
    if(shmid < 0){

        perror("shmget");
        sgsDeleteDataInfo(dataInfoPtrHead,-1);
        return -2;

    }
    /*
     * Now we attach the segment to our data space.
     */
    if ((shm = (void *) shmat(shmid, NULL, 0)) == (char *) -1) 
    {

        perror("shmat");
        sgsDeleteDataInfo(dataInfoPtrHead,-1);
        return -2;

    }
    if(CreateShm) memset(shm, 0, sizeof(struct dataInfo)*line);

    deviceInfoHead = deviceInfoPtr;
    dataInfoPtrTemp = deviceInfoHead->dataInfoPtr;
    shareMemPtr = (struct shareMem *)shm;
    while(deviceInfoHead != NULL)
    {

        //Initialize the mutex attributes

        if(CreateShm)
        {

            pthread_mutexattr_init(&mutex_attr);
            pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
            pthread_condattr_init(&cond_attr);
            pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

        }

        if(CreateShm)
        {

            
            pthread_mutex_init(&(shareMemPtr->lock), &mutex_attr);
            pthread_cond_init(&(shareMemPtr->lockCond), &cond_attr);


            ret = pthread_mutex_unlock( &(shareMemPtr->lock) );
            shareMemPtr->valueType = INITIAL_VALUE;
            if(ret != 0)
            {

                perror("sgsInitDataInfo");
                sgsDeleteDataInfo(dataInfoPtrHead, shmid);
                return -3;

            }
                
        
        }

        //locate shared memory address at here

        while(dataInfoPtrTemp == NULL)
        {

            //printf("getchaaa\n");
            deviceInfoHead = deviceInfoHead->next;
            if(deviceInfoHead != NULL)
                dataInfoPtrTemp = deviceInfoHead->dataInfoPtr;
            else break;

        }
        if(dataInfoPtrTemp != NULL)
        {

            //printf("gecha\n");
            dataInfoPtrTemp->shareMemoryPtr = shareMemPtr;
            shareMemPtr++;
            dataInfoPtrTemp = dataInfoPtrTemp->next;

        }
        

    }
    *dataInfoPtr = dataInfoPtrHead;

    //printf("Complete~~~~~~~~~~~~~~~\n\n");

    return shmid;

}

void sgsShowDeviceInfo(deviceInfo *deviceInfoPtr)
{
    deviceInfo *head = deviceInfoPtr;

    printf("\n");
    if(head == NULL)
    {

        printf("[%s,%d]input is NULL\n",__FUNCTION__,__LINE__);
        return;

    }
    else
    {
        printf(BROWN"Device Name     : %s\n",head->deviceName);
        printf("Interface       : %s\n",head->interface);
        printf("Protocol Config : %s\n",head->protocolConfig);
        printf("Description     : %s\n"NONE,head->description);
        printf("\n");

    }

    return;

}

void sgsShowDataInfo(dataInfo *dataInfoPtr)
{
    dataInfo *head = dataInfoPtr;
    dataLog dest;
    int ret = 0;
    char buf[128];

    while(head != NULL)
    {

        printf("\n");
        printf("\tSensor ID   : %d \n",head->ID);
        printf("\tDevice Name : %s\n",head->deviceName);
        printf("\tSensor Name : %s\n",head->sensorName);
        printf("\tValue  Name : %s\n",head->valueName);
        printf(LIGHT_GRAY"\t\tModbus ID          %u\n",head->modbusInfo.ID);
        printf("\t\tModbus Address     %u\n",head->modbusInfo.address);
        printf("\t\tModbus read Length %d\n"NONE,head->modbusInfo.readLength);
#if 1
        ret = sgsReadSharedMemory(head,&dest);
        //printf("[%s,%d] ret is %d\n",__FUNCTION__,__LINE__,ret);
        if(ret != 0)
        {

            printf("Read %s shm value failed\n",head->valueName);

        }
        else
        {
            
            memset(buf,'\0',sizeof(buf));
            //printf("[%s,%d] memset done\n",__FUNCTION__,__LINE__);
            strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &(dest.updatedTime));
            //printf("[%s,%d] strftime done\n",__FUNCTION__,__LINE__);
            printf(YELLOW"\t\t\tupdated time : %s\n"NONE,buf);
            //printf("valueType %u\n",dest.valueType);
            printf(LIGHT_RED"\t\t\tstatus = %d\n"NONE,dest.status);
            switch(dest.valueType)
            {

                case INITIAL_VALUE :
                    printf(LIGHT_GREEN"\t\t\tvalue : %s\n"NONE,dest.value.s);
                    break;

                case INTEGER_VALUE :
                    printf(LIGHT_GREEN"\t\t\tvalue : %d\n"NONE,dest.value.i);
                    break;

                case FLOAT_VALUE :
                    printf(LIGHT_GREEN"\t\t\tvalue : %f\n"NONE,dest.value.f);
                    break;

                case STRING_VALUE :
                    printf(LIGHT_GREEN"\t\t\tvalue : %s\n"NONE,dest.value.s);
                    break;

                case ERROR_VALUE :
                    printf(LIGHT_GREEN"\t\t\tvalue : %s\n"NONE,dest.value.s);
                    break;

                default:
                    printf("\t\t\tvalue : %s\n",dest.value.s);
                    printf("\t\t\t[Error] Unknown valueType %d\n",dest.valueType);
                    break;
 

            }

        }
#endif
        printf("\n");
        head = head->next;

    }
    return ;

}

void sgsShowAll(deviceInfo *deviceInfoPtr)
{

    deviceInfo *temp = deviceInfoPtr;
    while(temp != NULL)
    {

        sgsShowDeviceInfo(temp);
        if(temp->dataInfoPtr != NULL)
        {

            sgsShowDataInfo(temp->dataInfoPtr);

        }
        //printf("[%s,%d] getting next dataInfoPtr\n",__FUNCTION__,__LINE__);
        temp = temp->next;

    }

}

int sgsReadSharedMemory(dataInfo *dataInfoPtr, dataLog *dest)
{

    int timeout = 30;
    struct shareMem *shmPtr = dataInfoPtr->shareMemoryPtr;

    //we try 30 times when accessing the shared memory

    while(timeout-- > 0)
    {
        
        //printf("[%s,%d]running loop\n",__FUNCTION__,__LINE__);
        
        if(pthread_mutex_trylock( &(shmPtr->lock) ) != 0)
        {

            printf("[%s,%d]%s %s is busy\n", __FUNCTION__, __LINE__, dataInfoPtr->sensorName, dataInfoPtr->valueName);
            usleep(50000);
            continue;

        }
        else
        {

            //printf("[%s,%d] else\n",__FUNCTION__,__LINE__);

            //copy time struct

            dest->updatedTime.tm_sec = shmPtr->updatedTime.tm_sec;
            dest->updatedTime.tm_min = shmPtr->updatedTime.tm_min;
            dest->updatedTime.tm_hour = shmPtr->updatedTime.tm_hour;
            dest->updatedTime.tm_mday = shmPtr->updatedTime.tm_mday;
            dest->updatedTime.tm_mon = shmPtr->updatedTime.tm_mon;
            dest->updatedTime.tm_year = shmPtr->updatedTime.tm_year;
            dest->updatedTime.tm_wday = shmPtr->updatedTime.tm_wday;
            dest->updatedTime.tm_yday = shmPtr->updatedTime.tm_yday;
            dest->updatedTime.tm_isdst = shmPtr->updatedTime.tm_isdst;

            //Set status

            dest->status = shmPtr->status;

            //Set valueType
            
            dest->valueType = shmPtr->valueType;

            //printf("[%s,%d] shmPtr->valueType is %d \n",__FUNCTION__,__LINE__,shmPtr->valueType);

            switch(shmPtr->valueType)
            {

                case INITIAL_VALUE :
                    sprintf(dest->value.s,"Value is not ready yet");
                    break;

                case INTEGER_VALUE :
                    dest->value.i = shmPtr->value.i;
                    break;

                case FLOAT_VALUE :
                    dest->value.f = shmPtr->value.f;
                    break;

                case STRING_VALUE :
                    strncpy(dest->value.s, shmPtr->value.s, 128);
                    dest->value.s[127] = '\0';
                    break;

                case ERROR_VALUE :
                    strncpy(dest->value.s, shmPtr->value.s, 128);
                    dest->value.s[127] = '\0';
                    break;

                default:
                    strncpy(dest->value.s,shmPtr->value.s,128);
                    dest->value.s[127] = '\0';
                    printf("Unknown valueType %d\n",shmPtr->valueType);
                    break;

            }

            //Remember to unlock it 
            //printf("[%s,%d] switch done\n",__FUNCTION__,__LINE__);
            pthread_mutex_unlock( &(shmPtr->lock));
            //printf("[%s,%d] unlock\n",__FUNCTION__,__LINE__);

            return 0;
            
        }

    }
    printf("[%s,%d]Timeout!! lock %s %s is busy\n", __FUNCTION__, __LINE__, dataInfoPtr->sensorName, dataInfoPtr->valueName);
    return -1;

}

int sgsWriteSharedMemory(dataInfo *dataInfoPtr, dataLog *source)
{
    int timeout = 30;
    struct shareMem *shmPtr = dataInfoPtr->shareMemoryPtr;
    time_t now;
    DATETIME tmp_tm ;
    //We try 30 times when accessing the shared memory

    while(timeout-- > 0)
    {

        if(pthread_mutex_trylock( &(shmPtr->lock) ) != 0)
        {

            printf("[%s,%d]%s %s is busy\n", __FUNCTION__, __LINE__, dataInfoPtr->sensorName, dataInfoPtr->valueName);
            usleep(50000);
            continue;

        }
        else 
        {

            //Get current time and get struct tm type of the local time

            time(&now);
            tmp_tm = *localtime(&now);

            //Update our struct tm in shared memory

            shmPtr->updatedTime.tm_sec = tmp_tm.tm_sec;
            shmPtr->updatedTime.tm_min = tmp_tm.tm_min;
            shmPtr->updatedTime.tm_hour = tmp_tm.tm_hour;
            shmPtr->updatedTime.tm_mday = tmp_tm.tm_mday;
            shmPtr->updatedTime.tm_mon = tmp_tm.tm_mon;
            shmPtr->updatedTime.tm_year = tmp_tm.tm_year;
            shmPtr->updatedTime.tm_wday = tmp_tm.tm_wday;
            shmPtr->updatedTime.tm_yday = tmp_tm.tm_yday;
            shmPtr->updatedTime.tm_isdst = tmp_tm.tm_isdst;

            //Set status

            shmPtr->status = source->status;

            //Set value
            
            shmPtr->valueType = source->valueType;
            switch(source->valueType)
            {

                case INITIAL_VALUE :
                    sprintf(shmPtr->value.s,"Value is not ready yet");
                    break;

                case INTEGER_VALUE :
                    shmPtr->value.i = source->value.i;
                    break;

                case FLOAT_VALUE :
                    source->value.f = shmPtr->value.f;
                    break;

                case STRING_VALUE :
                    strncpy(shmPtr->value.s, source->value.s, 128);
                    shmPtr->value.s[127] = '\0';
                    break;

                case ERROR_VALUE :
                    strncpy(shmPtr->value.s, source->value.s, 128);
                    shmPtr->value.s[127] = '\0';
                    break;

                default:
                    strncpy(shmPtr->value.s, source->value.s, 128);
                    shmPtr->value.s[127] = '\0';
                    printf("Unknown valueType %d\n",source->valueType);
                    break;

            }

            //Remember to unlock it 

            pthread_mutex_unlock( &(shmPtr->lock));

            return 0;

        }

    }

    printf("[%s,%d]Timeout!! lock %s %s is busy\n", __FUNCTION__, __LINE__, dataInfoPtr->sensorName, dataInfoPtr->valueName);
    return -1;

}

int sgsCreateMsgQueue(key_t key, int create)
{

	int msgid;
	msgid=msgget(key, /*IPC_NOWAIT |*/ IPC_CREAT | 0600);

	if(msgid == -1)
    {
		printf("Open queue failed, msgget return %d\n",msgid);
		return -1;
	}
	printf("Open queue successfully id is %d\n",msgid);

	return msgid;
}

int sgsDeleteMsgQueue(int msgid)
{
	if (msgid == -1) 
    {
		printf("Message queue does not exist.\n");
		return 0;
	}

	if (msgctl(msgid, IPC_RMID, NULL) == -1) 
    {
		fprintf(stderr, "Message queue could not be deleted.\n");
		return -1;
	}

	printf("Message queue was deleted.\n");

	return 0;
}

int sgsSendQueueMsg(int msgid, char *message, int msgtype )
{
	struct msgbuff ptr ;
	int result =0;

	if(strlen(message) > 1024)
    {
		printf("The message is too long\n");
		return -1;
	}
	printf("Prepare to send a message to queue (type %d)\n",msgtype);
	ptr.mtype=(long)msgtype;

	strcpy(ptr.mtext,message);
	printf("ptr %ld %s\n",ptr.mtype,ptr.mtext);
	result = msgsnd(msgid,&ptr,sizeof(struct msgbuff)-sizeof(long),IPC_NOWAIT);
	if(result == -1)
    {
		perror("result");
		return -1;
	}
	printf("msgsnd return: %d \n",result);
	return result;
}

int sgsRecvQueueMsg(int msgid, char *buf, int msgtype )
{
	struct msgbuff ptr ;
	int result = 0;
	if(buf == NULL)
    {
		printf("The buf is NULL\n");
		return -1;
	}
	printf("Prepare to receive a message from queue\n");
	ptr.mtype=(long)msgtype;
	result = msgrcv(msgid,&ptr,sizeof(struct msgbuff) - sizeof(long),msgtype,IPC_NOWAIT);//IPC_NOWAIT);	//recv msg type=1
	if(result == -1)
    {
		printf("Currently no queue message available\n");
		perror("msgrcv");
		return -1;
	}
	else
    {
		printf("Queue message received, length %d bytes, type is %ld\n",result,ptr.mtype);
		printf("message:\n%s",ptr.mtext);
		strcpy(buf,ptr.mtext);
		return 0;
	}

}
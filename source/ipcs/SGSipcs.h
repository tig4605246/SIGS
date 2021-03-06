/*

    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 29,2017
    Program statement: 
        we wrap ipcs at here, including: 
        shared memory
        message queue

    update :

        2017/03/29 : Attach dataInfoPtr to deviceInfoPtr at sgsInitDataInfo

*/

#ifndef DEFINITIONS
#define DEFINITIONS

#include "../definition/SGSdefinitions.h"
#endif



// Intent : Free all allocated dataInfo at the pointer
// Pre : deviceInfo pointer, close type 
//       if the shmid > 0 , it'll delete the shared memory, if == -1, it'll simply detach it
// Post : On success, return. On error, -1 is returned

void sgsDeleteDataInfo(dataInfo *dataInfoPtr, int shmid);



// Intent : Free all allocated deviceInfo struct at the pointer 
// Pre : deviceInfo pointer,
// Post : On success, return. On error, -1 is returned

void sgsDeleteDeviceInfo(deviceInfo *deviceInfoPtr);



// Intent : Free all allocated resources 
// Pre : deviceInfo pointer, shmid (set to 0 for only detaching)
// Post : On success, return. On error, -1 is returned

void sgsDeleteAll(deviceInfo *deviceInfoPtr, int shmid);



// Intent : Create and initialize the deviceInfo with device.conf file 
// Pre : deviceInfo pointer
// Post : On success, return 0. On error, -1 is returned

int sgsInitDeviceInfo(deviceInfo **deviceInfoPtr);



// Intent : Create and initialize the dataInfo with data.conf file. After that, create a new shared memory or open an existed one
// Pre : deviceInfo pointer, open type (1 is new, 0 is open an exist one)
// Post : On success, return 0. On error, -1 is returned

int sgsInitDataInfo(deviceInfo *deviceInfoPtr, dataInfo **dataInfoPtr, int CreateShm);



// Intent : Display the content of the DeviceInfo pointed by the input pointer 
// Pre : deviceInfo pointer
// Post : nothing

void sgsShowDeviceInfo(deviceInfo *deviceInfoPtr);




// Intent : Display the content of the every dataInfo, started from the input pointer 
// Pre : dataInfo pointer
// Post : nothing
void sgsShowDataInfo(dataInfo *dataInfoPtr);



// Intent : Display the content in DeviceInfo, DataInfo and shared memory
// Pre : deviceInfo pointer
// Post : nothing

void sgsShowAll(deviceInfo *deviceInfoPtr);



// Intent : Get data from shared memory
// Pre : dataInfoPtr pointer
// Post : On success, return 0. On error, -1 is returned

int sgsReadSharedMemory(dataInfo *dataInfoPtr, dataLog *dest);



// Intent : Write data to shared memory
// Pre : dataInfoPtr pointer
// Post : On success, return 0. On error, -1 is returned

int sgsWriteSharedMemory(dataInfo *dataInfoPtr, dataLog *source);



// Intent : Create (or open if it's already exist) a message queue 
// Pre : key and open type
// Post : On success, return msg queue's id. On error, -1 is returned

int sgsCreateMsgQueue(key_t key, int create);



// Intent : Delete  a message queue 
// Pre : message queue id
// Post : On success, return 0. On error, -1 is returned

int sgsDeleteMsgQueue(int msgid);



// Intent : send a message to queue 
// Pre : message queue id, pointer to message, type of the message
// Post : On success, return 0. On error, -1 is returned

int sgsSendQueueMsg(int msgid, char *message, int msgtype );



// Intent : receive a message from queue 
// Pre : message queue id, pointer to store the message, type of the message
// Post : On success, return 0. On error, -1 is returned

int sgsRecvQueueMsg(int msgid, char *buf, int msgtype );




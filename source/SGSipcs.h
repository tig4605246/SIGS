/*

    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 2,2017
    Program statement: 
        we wrap ipcs at here, including: 
        shared memory
        message queue

*/

#include "SGSdefinitions.h"

// the key we use to generate ipcs

#define SGSKEY 53595

//the path we use to generate ipcs

#define SGSPATH "./"

#define OPENNEWSHM 1

#define OPENEXISTSHM 0

// Intent : Create and initialize the deviceInfo with device.conf file 
// Pre : deviceInfo pointer
// Post : On success, return 0. On error, -1 is returned

int sgsInitDeviceInfo(deviceInfo *deviceInfoPtr);

// Intent : Create and initialize the dataInfo with data.conf file. After that, create a new shared memory or open an existed one
// Pre : deviceInfo pointer, open type (1 is new, 0 is open an exist one)
// Post : On success, return 0. On error, -1 is returned

int sgsInitDataInfo(deviceInfo *deviceInfoPtr, int create);

// Intent : Free all allocated resources 
// Pre : deviceInfo pointer, close type 
//       (if create == 1, it'll delete the shared memory, if create == 0, it'll simply detach the shared memory, if create == -1, it won't do anything with shared memory )
// Post : On success, return. On error, -1 is returned

int sgsDeleteAll(deviceInfo *deviceInfoPtr, int create);

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
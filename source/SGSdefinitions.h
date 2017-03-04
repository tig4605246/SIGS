/*

    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 1,2017
    Program statement: 
        In here, We define the struct and some general variables, parameters

        After that, we define some datalog format

*/


#include <stdint.h>
#include <pthread.h>

//defining time variable

#ifndef DATETIME
#define DATETIME uint64_t
#endif

//this struct is for queue to use

struct msgbuff
{
	long mtype;
	char mtext[512];
};

typedef struct msgbuff msgbuff;

//this structure is used to store the Modbus info

struct modbusInfo
{
    
    //Modbus Sensor ID

    unsigned char ID;

    //Modbus read address

    unsigned int  address;

    //Modbus read length, a good thing for debugging

    int readLength;

    //Modbus response 

    unsigned char response[64];

    //formatted command

    unsigned char cmd[64];

}

typedef struct modbusInfo modbusInfo;

//this union is used to store data value

union dataValue
{
    
    int i;

    float f;

    char s[64];

}

typedef union dataValue dataValue;

//this structure is used to store shared memory information

struct shareMem
{

    //update time

    DATETIME updatedTime;

    //value type

    unsigned int valueType;

    //value 

    union dataValue value;

    //inter-rpocess mutex lock

    pthread_mutex_t shmMutex;

    //pointer to the certain place of the shared memory

    void *shmPtr;

}

typedef struct shareMem shareMem;

//this structure is used to store data information

struct dataInfo
{

    //Sensor ID 

    unsigned int ID;

    //name of the device which this data comes from

    char deviceName[32];

    //name of the sensor where data came from 

    char sensorName[32];

    //value name

    char valueName[32];

    //pointer to struct that stores modbus info 

    struct modbusInfo *modbusInfoPtr;

    //pointer to the struct that store shared memory's info 

    struct shareMem *shareMemoryPtr;

    //pointer to next dataInfo struct

    struct dataInfo *next;


}

typedef struct dataInfo dataInfo;

//this structure is used to sort the dataInfo by its source

struct deviceInfo
{

    //name of the device
    
    char deviceName[32];

    char interface[32];

    /*

        device's protocol info
        For example,
            Modbus config: 19200,8n1
            TCP config: 140.118.70.136,9876

    */

    char protocolConfig[32];

    //points to the data related to this device

    char dataInfo *next;

    //pointer to whole shared memory

    void *sharedMemPtr;

}

typedef struct deviceInfo deviceInfo;

/*

    this area is for datalogs

*/

//default a max length of the datalogs, make it to be a standard

#define MAXDATALENGTH 250



struct dataLog
{

    //name of the sensor where data came from 

    char sensorName[32];

    //value name

    char valueName[32];

    //value type

    unsigned int type;

    //data value

    union value;

}

typedef struct dataLog dataLog;


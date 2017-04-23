/*

    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 22,2017
    Program statement: 
        In here, We define the struct and some general variables, parameters

        After that, we define some datalog format
    Edited:
        Adding uploadInfo for controlling use

*/


#include <stdint.h>
#include <pthread.h>



//Build version of the SIGS

#define PROJECTSTATUS  "Alpha Build"
#define PROJECTVERSION "0.5"

//defines the log system type

#define SQLITE3
#define BUFFERPERIOD 7

//defines the time variable

#ifndef DATETIME

typedef struct tm tm;

typedef time_t epochTime;

#define DATETIME tm

#endif

//defining value type

#define INITIAL_VALUE 0x00000000
#define INTEGER_VALUE 0x00000010
#define FLOAT_VALUE   0x00000011
#define STRING_VALUE  0x00000100
#define ERROR_VALUE   0x00001000
#define UNKNOWN_VALUE 0x00001001

//define Data size

#define DATAVALUEMAX 128

#define MSGBUFFSIZE 512

//define the log path of device.conf and data.conf

#define CONFIGURECONF "../conf/configure.conf"
#define DEVICECONF    "../conf/device.conf"
#define DATACONF      "../conf/data.conf"

// Master key for generating ipcs

#define SGSKEY 53595123

// General definitions

#define SGSPATH "./"
#define OPENNEWSHM 1
#define OPENEXISTSHM 0 

//this struct is used by queue message 

struct msgbuff
{
	long mtype;
	char mtext[MSGBUFFSIZE];
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

    //a char for optional selection. For example, taida's deltarpi-m15 and m30 series

    int option;

    //Modbus response 

    unsigned char response[DATAVALUEMAX];

    //formatted command

    unsigned char cmd[DATAVALUEMAX];

};

typedef struct modbusInfo modbusInfo;

//this union is used to store data value

union dataValue
{
    
    int i;

    float f;

    char s[DATAVALUEMAX];

};

typedef union dataValue dataValue;

//this structure is used to store shared memory information

struct shareMem
{

    //update time

    DATETIME updatedTime;

    //Status

    int status;

    //value type

    int valueType;

    //value 

    dataValue value;

    //inter-process mutex lock

    pthread_mutex_t lock;

    pthread_cond_t  lockCond;

};

typedef struct shareMem shareMem;

//this structure is used to store data information

struct dataInfo
{

    //Sensor ID 

    unsigned int ID;

    //name of the device which this data comes from

    char deviceName[DATAVALUEMAX];

    //name of the sensor where data came from 

    char sensorName[DATAVALUEMAX];

    //value name

    char valueName[DATAVALUEMAX];

    //struct that stores modbus info 

    struct modbusInfo modbusInfo;

    //pointer to the struct that store shared memory's info 

    struct shareMem *shareMemoryPtr;

    //pointer to next dataInfo struct

    struct dataInfo *next;


};

typedef struct dataInfo dataInfo;

//this structure is used to sort the dataInfo by its source

struct deviceInfo
{

    //name of the device
    
    char deviceName[DATAVALUEMAX];

    char interface[DATAVALUEMAX];

    /*

        device's protocol info
        For example,
            Modbus config: 19200-8n1
            TCP config: 140.118.70.136-9876

    */

    char protocolConfig[DATAVALUEMAX];

    char description[DATAVALUEMAX];

    pid_t subProcessPid;

    //points to the data related to this device

    dataInfo *dataInfoPtr;

    //point to next deviceInfo structure

    struct deviceInfo *next;

};

typedef struct deviceInfo deviceInfo;

//this structure is used to store uplaoder's info

struct uploadInfo
{

    pid_t uploadProcessPid;

    char uploadProcessName[DATAVALUEMAX];

    struct uploadInfo *next;

};

typedef struct uploadInfo uploadInfo;

/*

    this is for datalogs

    A reduced format for shareMem

*/

struct dataLog
{

    //update time

    DATETIME updatedTime;

    //data status 1 is valid, 0 is invalid

    int status;

    //name of the sensor where data came from 

    char sensorName[DATAVALUEMAX];

    //value name

    char valueName[DATAVALUEMAX];

    //value type

    unsigned int valueType;

    //data value

    dataValue value;

};

typedef struct dataLog dataLog;

//Intetnt : show current version
//Pre : Nothing
//Post : Nothing

void showVersion();
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

#define PROJECTSTATUS "Alpha Build"
#define PROJECTVERSION "0.3"

//defining time variable

#ifndef DATETIME
#define DATETIME uint64_t
#endif

//defining value type

#define INITIAL_VALUE 0x00000000
#define INTEGER_VALUE 0x00000010
#define FLOAT_VALUE 0x00000011
#define STRING_VALUE 0x00000100

#define ERROR_VALUE 0x00001000
#define UNKNOWN_VALUE 0x00001001

//define max data union size

#define DATAVALUEMAX 128

//define a max length of the datalogs, make it to be a standard

#define MAXDATALENGTH 250

//define the log path of device.conf and data.conf

#define CONFIGURECONF "../conf/configure.conf"

#define DEVICECONF "../conf/device.conf"
#define DATACONF "../conf/data.conf"

// the key we use to generate ipcs

#define SGSKEY 53595

//the path we use to generate ipcs

#define SGSPATH "./"

#define OPENNEWSHM 1

#define OPENEXISTSHM 0 

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

    char deviceName[32];

    //name of the sensor where data came from 

    char sensorName[32];

    //value name

    char valueName[32];

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
    
    char deviceName[128];

    char interface[128];

    /*

        device's protocol info
        For example,
            Modbus config: 19200-8n1
            TCP config: 140.118.70.136-9876

    */

    char protocolConfig[128];

    char description[128];

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

    char uploadProcessName[32];

    struct uploadInfo *next;

};

typedef struct uploadInfo uploadInfo;

/*

    this is for datalogs

    A reduced format for shareMem

*/

struct dataLog
{

    //name of the sensor where data came from 

    char sensorName[32];

    //value name

    char valueName[32];

    //value type

    unsigned int valueType;

    //data value

    dataValue value;

};

typedef struct dataLog dataLog;


/*

    Planing to use SQLite for log file storage
    This will help me coupling with it

*/

struct logFile_DataType
{

    int amountOfDataLogs;

    DATETIME recordTime;

    dataLog *dataLogs;

};

typedef struct logFile_DataType logFile_DataType;

//Intetnt : show current version
//Pre : Nothing
//Post : Nothing

void showVersion();
/*

    Name: Xu Xi-Ping
    Date: April 18,2017
    Last Update: April 18,2017
    Program statement: 
        define the funtions for recording logs, retreiving logs and list logs.

*/

#ifndef DEFINITIONS
#define DEFINITIONS

#include "../definition/SGSdefinitions.h"
#endif

#ifndef IPCS
#define IPCS

#include "../ipcs/SGSipcs.h"
#endif

#include "../thirdparty/sqlite3.h"

#ifndef EVENT
#define EVENT


#include "../events/SGSEvent.h"
#endif 

//Whether using SQLITE3 or json format log, it's decided by the SGSdefinitions 

#ifdef SQLITE3

//purpose : This function will open the SQlite databse or create a new one if it's not existed.
//Pre : .db filename, ** pointer of sqlite3 
//Post : On success, return 0, otherwise a -1 will be returned

int sgsOpenSqlDB(char *fileName , sqlite3 **db);


//Purpose : This function will create datatable at the giving database
//Pre : db pointer, deviceInfo pointer of ther target device
//Post : On success, return 0, Ohterwise return -1

int sgsCreateTable(sqlite3 *db, deviceInfo *target);

//Purpose : This function will new a data record to the giving database with the data we have in the giving deviceInfo
//Pre : db pointer, deviceInfo pointer of the target device
//Post : On success, return 0, otherwise a -1 will be returned

int sgsNewRecord(sqlite3 *db, deviceInfo *target);

//Purpose : This function will retreive all records belongs to the giving device
//Pre : db pointer, target device
//Post : On success, return 0, otherwise -1 is return.

int sgsRetreiveAllRecord(sqlite3 *db, deviceInfo *target);

//Purpose : Retreive records which Timestamp are smaller than giving epoch time.  (ceiling)
//Pre : db pointer, target device
//Post : On success, return 0, otherwise -1 is return.

int sgsRetreiveRecordsByTime(sqlite3 *db, deviceInfo *target, epochTime selectedTime);

//Purpose : Delete records which is earlier by giving epoch time
//Pre : db pointer, target device
//Post : On success, return 0, otherwise -1 is return.

int sgsDeleteRecordByTime(sqlite3 *db, deviceInfo *target, epochTime selectedTime);


#else


#endif

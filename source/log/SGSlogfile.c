/*

    Name: Xu Xi-Ping
    Date: April 18,2017
    Last Update: April 18,2017
    Program statement:
        implement the functions declared in SGSlogfile.h

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "SGSlogfile.h"



//A default callback function

static int sgsDefaultCallback(void *NotUsed, int argc, char **argv, char **azColName)
{

   int i;
   for(i=0; i<argc; i++)
   {

      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");

   }
   printf("\n");
   return 0;

}

int sgsOpenSqlDB(char *fileName , sqlite3 **db)
{

    char *zErrMsg = 0;
    int ret;

    ret = sqlite3_open(fileName, db);

    if( ret )
    {

        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
        return -1;

    }
    else
    {

        fprintf(stderr, "Opened database successfully\n");
        
    }

    return 0;

}

int sgsCreateTable(sqlite3 *db, dataInfo *target)
{

    dataInfo *head = target;
    dataInfo *data = target;
    dataInfo *tmp = target;
    int columnCount = 0;
    int i = 0;
    int ret = 0;
    char *table = NULL;
    char *zErrMsg = NULL;
    char buf[DATAVALUEMAX];
    char sensorName[DATAVALUEMAX];

    //Before doing anything, check the input first

    if(target == NULL)
    {

        printf(LIGHT_RED"[%s,%d] target is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    } 
    if(data == NULL)
    {

        printf(LIGHT_RED"[%s,%d] data is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }
    if(db == NULL)
    {

        printf(LIGHT_RED"[%s,%d] db is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }

    //Count how many columns we have

    memset(sensorName,'\0',sizeof(sensorName));
    strncpy(sensorName,tmp->sensorName,sizeof(sensorName)-1);

    while((tmp != NULL) && !strcmp(sensorName,tmp->sensorName))
    {
        columnCount++;
        tmp = tmp->next;
    }
    if(columnCount == 0)
    {
        printf(LIGHT_RED"[%s,%d] There's no data belongs to this device.\n"NONE,__FUNCTION__,__LINE__);
        return -1;
    }

    //Construct the sql command for creating table

    table = (char *)malloc(sizeof(char ) * DATAVALUEMAX * (columnCount + 1) );

    if(table == NULL)
    {

        printf(LIGHT_RED"[%s,%d] malloc failed\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }

    //Clear the char array

    memset(table,'\0',sizeof(table));
    memset(buf,'\0',sizeof(buf));

    //Start to fill the command

    snprintf(buf,DATAVALUEMAX,"CREATE TABLE %s(",target->deviceName);

    strcat(table,buf);

    snprintf(buf,DATAVALUEMAX,"Timestamp NUMERIC         NOT NULL,");

    strcat(table,buf);

    snprintf(buf,DATAVALUEMAX,"sensorName    CHAR(%d)    NOT NULL,",DATAVALUEMAX);

    strcat(table,buf);


    i = 0;

    //Fill the rest of the command

    while((i < columnCount) && (data != NULL))
    {

        if(i == (columnCount-1))
            snprintf(buf,DATAVALUEMAX,"`%s`    CHAR(%d)    NOT NULL,",data->valueName,DATAVALUEMAX);
        else
            snprintf(buf,DATAVALUEMAX,"`%s`    CHAR(%d)    NOT NULL,",data->valueName,DATAVALUEMAX);

        strcat(table,buf);
        data = data->next;
        i++;

    }

    snprintf(buf,DATAVALUEMAX,"PRIMARY KEY (Timestamp, sensorName));");

    strcat(table,buf);


    printf("table is : \n%s\n\n",table);

    printf("[%s,%d] i is %d (columnCount ) is %d\n",__FUNCTION__,__LINE__,i,(columnCount ));

    ret = sqlite3_exec(db, table, sgsDefaultCallback, 0, &zErrMsg);
    if( ret != SQLITE_OK )
    {

        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);

    }else
    {

        fprintf(stdout, "Table created successfully\n");

    }

    free(table);
    return 0;

}

int sgsNewRecord(sqlite3 *db, dataInfo *target, cF callbackFunction)
{

    dataInfo *head = target;
    dataInfo *data = target;
    dataInfo *tmp = target;
    dataLog dLog;
    int rowCount = 0;
    int i = 0;
    int ret = 0;
    char *table = NULL;
    char *zErrMsg = NULL;
    char buf[256];
    char insertBuf[DATAVALUEMAX];
    char sensorName[DATAVALUEMAX];

    printf("hi~~~\n");

    //Before doing anything, check the input first

    if(target == NULL)
    {

        printf(LIGHT_RED"[%s,%d] target is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }
    else if(target == NULL)
    {

        printf(LIGHT_RED"[%s,%d] target->dataInfoPtr is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    } 
    if(data == NULL)
    {

        printf(LIGHT_RED"[%s,%d] data is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }
    if(db == NULL)
    {

        printf(LIGHT_RED"[%s,%d] db is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }

    
    //Clear the char array

    memset(insertBuf,'\0',sizeof(insertBuf));
    memset(buf,'\0',sizeof(buf));

    //Start to fill the command

    snprintf(buf,256,"INSERT INTO %s (",target->deviceName);
    strcat(insertBuf,buf);

    snprintf(buf,256,"Timestamp,");

    strcat(insertBuf,buf);

    snprintf(buf,256,"sensorName,");

    strcat(insertBuf,buf);
    //printf("first insertBuf %s\n",insertBuf);

    //Get the sensorName to help us count the columns

    memset(sensorName,'\0',sizeof(sensorName));
    strncpy(sensorName,tmp->sensorName,sizeof(sensorName)-1);

    //We count column and fill some commands at the same time

    while((tmp != NULL) && (strcmp(sensorName,tmp->sensorName) == 0))
    {
        if(tmp != NULL)
        {
            //printf("valueName %s senserName %s tmpName %s\n",tmp->valueName,tmp->sensorName,sensorName);
            snprintf(buf,256,"%s,",tmp->valueName);
            //printf("valueName %s buf %s\n",tmp->valueName,buf);
            strcat(insertBuf,buf);
            //printf("insertBuf %s\n",insertBuf);
        }
        else
        {
            break;
        }
        
        tmp = tmp->next;

    }

    insertBuf[strlen(insertBuf) - 1] = 0;

    //Finished the INSERT table shape

    snprintf(buf,256,")");

    strcat(insertBuf,buf);
    
    //printf("line 278,insertBuf:\n%s\n",insertBuf);

    //Get rowCount

    tmp = data;
    memset(sensorName,'\0',sizeof(sensorName));

    while(tmp != NULL)
    {

        if(strcmp(tmp->sensorName,sensorName))
        {
            rowCount++;
            strncpy(sensorName,tmp->sensorName,sizeof(sensorName)-1);
        }
        tmp = tmp->next;

    }
    if(rowCount == 0)
    {
        printf(LIGHT_RED"[%s,%d] There's no data belongs to this device.\n"NONE,__FUNCTION__,__LINE__);
        return -1;
    }


    //Construct the sql command char array

    table = (char *)malloc(sizeof(char ) * DATAVALUEMAX * (rowCount * 2) );

    if(table == NULL)
    {

        printf(LIGHT_RED"[%s,%d] malloc failed\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }

    //printf(LIGHT_RED"[%s,%d] insertBuf : \n%s\n"NONE,__FUNCTION__,__LINE__,insertBuf);

    memset(table,'\0',sizeof(table));

    tmp = data;//Reset the tmp pointer

    printf("go to while\n");

    while(tmp != NULL)
    {

        //printf(LIGHT_RED"[%s,%d] while \n"NONE,__FUNCTION__,__LINE__);
        //Insert INTO sensorName (valueName....)

        strcat(table,insertBuf);

        //Update sensorName

        snprintf(sensorName,DATAVALUEMAX,"%s",tmp->sensorName);

        //Get record time

        snprintf(buf,256,"VALUES (strftime('%%s','now','localtime'),");

        strcat(table,buf);

        //Get sensorName

        snprintf(buf,256," '%s',",sensorName);

        strcat(table,buf);

        //Get sensor values
        
        while(!strcmp(tmp->sensorName,sensorName))
        {

            //printf(LIGHT_RED"[%s,%d] 2while \n"NONE,__FUNCTION__,__LINE__);
            //printf("table is : \n%s\n\n",table);
            memset(dLog.value.s,'\0',sizeof(dLog.value.s));
            memset(buf,'\0',sizeof(buf));
            sgsReadSharedMemory(tmp,&dLog);
            //printf(LIGHT_RED"[%s,%d] shm read done\n"NONE,__FUNCTION__,__LINE__);

            switch(dLog.valueType)
            {

                case INTEGER_VALUE :
                    snprintf(buf,256," '%d'",dLog.value.i);
                    //printf(LIGHT_RED"[%s,%d] INTEGER \n"NONE,__FUNCTION__,__LINE__);
                    //printf(LIGHT_GREEN"[%s,%d] dLog.value.i %d \nbuf is :\n%s\n"NONE,__FUNCTION__,__LINE__,dLog.value.i,buf);
                    strcat(table,buf);
                    //printf(LIGHT_PURPLE"INGETER : \ntable is : \n%s\n\n"NONE,table);
                    break;

                case FLOAT_VALUE :
                    snprintf(buf,256," '%f'",dLog.value.f);
                    //printf(LIGHT_RED"[%s,%d] FLOAT \n"NONE,__FUNCTION__,__LINE__);
                    strcat(table,buf);
                    break;

                case STRING_VALUE :
                    snprintf(buf,256," '%s'",dLog.value.s);
                    //printf(LIGHT_RED"[%s,%d] STRING \n"NONE,__FUNCTION__,__LINE__);
                    strcat(table,buf);
                    break;

                default :
                    //printf(LIGHT_GREEN"[%s,%d] dLog.value.s %s \nbuf is :\n%s\n"NONE,__FUNCTION__,__LINE__,dLog.value.s,buf);
                    snprintf(buf,256," '%s'",dLog.value.s);
                    //printf(LIGHT_RED"[%s,%d] NOT_READY \n"NONE,__FUNCTION__,__LINE__);
                    strcat(table,buf);
                    //printf(LIGHT_PURPLE"default : \ntable is : \n%s\n\n"NONE,table);
                    break;

            }

            tmp = tmp->next;
            if(tmp == NULL)
            {
                printf(LIGHT_RED"[%s,%d] strcmp \n"NONE,__FUNCTION__,__LINE__);
                snprintf(buf,256,");");
                strcat(table,buf);
                break;
            }
            else if(strcmp(tmp->sensorName,sensorName))
            {
                printf(LIGHT_RED"[%s,%d] strcmp \n"NONE,__FUNCTION__,__LINE__);
                snprintf(buf,256,");");
                strcat(table,buf);
            }
            else
            {
                memset(buf,'\0',sizeof(buf));
                snprintf(buf,256,", ");
                strcat(table,buf);
            }

        }
        usleep(100000);
        

    }
    
    printf("table is : \n%s\n\n",table);

    //printf("[%s,%d] i is %d (rowCount ) is %d\n",__FUNCTION__,__LINE__,i,(rowCount ));

    ret = sqlite3_exec(db, table, sgsDefaultCallback, 0, &zErrMsg);
    if( ret != SQLITE_OK )
    {

        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);

    }else
    {

        fprintf(stdout, "New record successfully\n");

    }

    free(table);

    
    return 0;

}

int sgsRetrieveRecordsByTime(sqlite3 *db, dataInfo *target, epochTime selectedTime, cF callbackFunction)
{

    dataInfo *head = target;
    dataInfo *data = target;
    dataLog dLog;
    int ret = 0;
    char *zErrMsg = NULL;
    char buf[DATAVALUEMAX];



    //Before doing anything, check the input first

    if(target == NULL)
    {

        printf(LIGHT_RED"[%s,%d] target is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    } 
    if(data == NULL)
    {

        printf(LIGHT_RED"[%s,%d] data is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }
    if(db == NULL)
    {

        printf(LIGHT_RED"[%s,%d] db is NULL\n"NONE,__FUNCTION__,__LINE__);
        return -1;

    }

    memset(buf,'\0',sizeof(buf));

    

    snprintf(buf,DATAVALUEMAX,"SELECT * FROM %s WHERE Timestamp < %ld",head->deviceName,selectedTime);
    printf(LIGHT_RED"[%s,%d]buf %s \n"NONE,__FUNCTION__,__LINE__,buf);

    //If ther user doesn't give us a callback function, we'll use a default one

    if(callbackFunction == NULL)
        ret = sqlite3_exec(db, buf, sgsDefaultCallback, 0, &zErrMsg);
    else
        ret = sqlite3_exec(db, buf, callbackFunction, 0, &zErrMsg);

    if( ret != SQLITE_OK )
    {

        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);

    }else
    {

        fprintf(stdout, "sgsRetrieveRecordsByTime done successfully\n");

    }
    return 0;



}

int sgsDeleteRecordsByTime(sqlite3 *db, dataInfo *target, epochTime selectedTime)
{

    char buf[DATAVALUEMAX];
    char *zErrMsg = 0;
    int ret = 0;


    if(target == NULL)
    {

        printf(LIGHT_RED"No target to delete the log\n"NONE);
        return -1;

    }

    memset(buf,0,sizeof(buf));

    snprintf(buf,DATAVALUEMAX,"DELETE from %s where Timestamp < strftime('%%s','now' ,'localtime' ,'-7 day');",target->deviceName);

    //snprintf(buf,DATAVALUEMAX,"DELETE from %s where Timestamp < strftime('%%s','now' ,'localtime', '-60 seconds');",target->deviceName);


    ret = sqlite3_exec(db, buf, sgsDefaultCallback, 0, &zErrMsg);
    
    if( ret != SQLITE_OK )
    {

        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        return -1;
    
    }
    else
    {

        fprintf(stdout, "Delete: Operation done successfully\n");
    
    }

    return 0;

}
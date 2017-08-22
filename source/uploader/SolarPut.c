/*

    Name: Xi-Ping Xu
    Date: July 19,2017
    Last Update: August 18,2017
    Program statement: 
        This is an agent used to test SGS system. 
        It has following functions:
        1. Get a data buffer info from the data buffer pool
        2. Show fake data.
        3. Issue Command to FakeTaida and receive result

*/

/*

    Process:
    
    1. Init

    2. Send the data to server regarding to a certain period of the time passed in by the SolarPost

    3. Leaving

*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <error.h>
#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../thirdparty/cJSON.h"
#include "../thirdparty/sqlite3.h" 
#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"

//execlp("./SolarPut","./SolarPut", Resend_time_s, Resend_time_e, address,NULL);

static int callback(void *data, int argc, char **argv, char **azColName);


//Post definitions for max length

#define SA      struct sockaddr
#define MAXLINE 16384
#define MAXSUB  16384

//Intent    : Get config from conf file. If failed, use the default one
//Pre       : Nothing
//Post      : Always return 0

int GetConfig();

//Intent    : Post data to server
//Pre       : Nothing
//Post      : On success, return 0, otherwise return -1

int PostToServer();

ssize_t process_http( char *content, char *address);

int my_write(int fd, void *buffer, int length);

int my_read(int fd, void *buffer, int length);

//Intent    : Process queue message
//Pre       : Nothing
//Post      : Always return 0

int CheckAndRespondQueueMessage();

int ShutdownSystemBySignal(int sigNum);

cJSON *jsonRoot = NULL;

dataInfo *dInfo[2] = {NULL,NULL};    // pointer to the shared memory
int interval = 10;  // time period between last and next collecting section
int eventHandlerId; // Message queue id for event-handler
int shmId;          // shared memory id
int msgId;          // created by sgsCreateMessageQueue
int msgType;        // 0 1 2 3 4 5, one of them

typedef struct postNode
{

    char GW_ver[16];        //string
    char IP[4][128];        //string
    epochTime Date_Time;    //time_t
    int Send_Rate;          //float to int
    int Gain_Rate;          //float to int
    int Backup_time;        //int
    char MAC_Address[32];   //string
    char Station_ID[16];    //string
    char GW_ID[16];         //string

}pNode;

pNode postConfig;

int main(int argc, char* argv[])
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    char sql[256], *unformatted = NULL;
    

    /* Open database */
    rc = sqlite3_open("./log/SGSdb.db", &db);
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return(0);
    }else{
        fprintf(stderr, "Opened database successfully\n");
    }

    /* Create SQL statement */

    memset(sql, 0, sizeof(sql));
    snprintf(sql, 255,  "SELECT * from SOLAR where Timestamp BETWEEN %s AND %s; ", argv[1], argv[2]);
    printf("command : %s \n",sql);

    /* Get Config from setting file */

    rc = GetConfig(); 

    /* Execute SQL statement */
    rc = sqlite3_exec(db, sql, callback, ((void*)(NULL)), &zErrMsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }else{
        fprintf(stdout, "Operation done successfully\n");
    }

    unformatted =  cJSON_Print(jsonRoot);

    printf("json:\n %s \n", unformatted);

    free(unformatted);

    cJSON_Delete(jsonRoot);

    sqlite3_close(db);
    return 0;
}

int GetConfig()
{

    FILE *fp = NULL;
    char buf[128];
    char *name;
    char *value;
    int i = 0;

    memset(&(postConfig),0,sizeof(postConfig));

    snprintf(postConfig.GW_ver, 15, "Alpha Build V1.0");

    snprintf(postConfig.IP[i], 127, "http://connectcloud.fetnet.net:9000/PV_rawdata");
    
    postConfig.Send_Rate = 30;

    postConfig.Gain_Rate = 30;

    postConfig.Backup_time = 60;

    snprintf(postConfig.MAC_Address, 31, "7c:b0:c2:4f:76:1c");
    snprintf(postConfig.Station_ID, 15, "T910142");
    snprintf(postConfig.GW_ID, 15, "01");

    fp = fopen("./conf/Upload/SolarPost", "r");

    if(fp == NULL)
    {

        printf("SolarPost can't open the setting file. I'll use the default value\n");
        return 0;

    }
    else
    {

        while(!feof(fp))
        {

            fgets(buf, 127, fp);

            name = strtok(buf, SPLITTER);

            if(!strcmp(name, "GW_ver"))
            {

                snprintf(postConfig.GW_ver, 15, "%s", value);

            }
            else if(!strcmp(name, "IP_1"))
            {

                snprintf(postConfig.IP[0], 127, "%s", value);

            }
            else if(!strcmp(name, "IP_2"))
            {

                snprintf(postConfig.IP[1], 127, "%s", value);

            }
            else if(!strcmp(name, "IP_3"))
            {

                snprintf(postConfig.IP[2], 127, "%s", value);

            }
            else if(!strcmp(name, "IP_4"))
            {

                snprintf(postConfig.IP[3], 127, "%s", value);

            }
            else if(!strcmp(name, "Send_Rate"))
            {

                postConfig.Send_Rate = atof(value) * 60;

            }
            else if(!strcmp(name, "Gain_Rate"))
            {

                postConfig.Gain_Rate = atof(value) * 60;

            }
            else if(!strcmp(name, "Backup_time"))
            {

                postConfig.Backup_time = atof(value);

            }
            else if(!strcmp(name, "MAC_Address"))
            {

                snprintf(postConfig.MAC_Address, 31, "%s", value);

            }
            else if(!strcmp(name, "Station_ID"))
            {

                snprintf(postConfig.Station_ID, 15, "%s", value);

            }
            else if(!strcmp(name, "GW_ID"))
            {

                snprintf(postConfig.GW_ID, 15, "%s", value);

            }

        }

    }   

    fclose(fp);
    
    return 0;

}

int PostToServer()
{

    dataInfo *tempInfo[2] = {dInfo[0], dInfo[0]};
    dataLog dLog;
    epochTime nowTime;
    char buf[256];
    char *jsonOutput = NULL;
    char *jsonBuff = NULL;
    cJSON *root = NULL;
    cJSON *rows = NULL;
    cJSON *inverter = NULL;
    cJSON *tempArray = NULL;
    cJSON *tempObj = NULL;
    int irrNum = 0;                 //How many irr we have
    int irrVal[100] = {0};
    int irrStat[100] = {0};
    int i = 0, count = 10, ret = -1;
    typedef struct sysNode
    {

        int cpuUsage;
        int memoryUsage;
        int storageUsage;
        int networkFlow;

    }sNode;

    sNode systemInfo;

    //The best steps to match the format is 
    //1. get system info
    //2. get solar info & form JSON at the same time
    //3. Post them
    //4. Process the return value

    systemInfo.cpuUsage = 0;
    systemInfo.memoryUsage = 0;
    systemInfo.storageUsage = 0;
    systemInfo.networkFlow = 0;

    //1. get system info

    if(tempInfo[1] == NULL)
    {

        printf(LIGHT_RED"Encounter some problem while accessing GWInfo's data buffer\n"NONE);
        memset(buf, 0, sizeof(buf));
        snprintf(buf, 255, "%s;Encounter some problem while accessing GWInfo's data buffer", ERROR);
        sgsSendQueueMsg(eventHandlerId, buf, msgId);

    }

    while(tempInfo[1] != NULL)
    {

        sgsReadSharedMemory(tempInfo[1], &dLog);
        if(!strcmp(tempInfo[1]->valueName, "CPU_Usage"))    systemInfo.cpuUsage = dLog.value.i;
        else if(!strcmp(tempInfo[1]->valueName, "Memory_Usage"))    systemInfo.memoryUsage = dLog.value.i;
        else if(!strcmp(tempInfo[1]->valueName, "Storage_Usage"))    systemInfo.storageUsage = dLog.value.i;
        else if(!strcmp(tempInfo[1]->valueName, "Network_Flow"))    systemInfo.networkFlow = dLog.value.i;

        tempInfo[1] = tempInfo[1]->next;

    }

    //2. get solar info & form JSON at the same time

    //First, get irr and temp, They'll be the firsts in dataInfo linked list

    tempArray = cJSON_CreateArray();

    i = 0;

    while(tempInfo[0] != NULL && (!strcmp(tempInfo[0]->sensorName, "Temperature") || !strcmp(tempInfo[0]->sensorName, "Irradiation")) )
    {

        if(!strcmp(tempInfo[0]->valueName, "Temperature"))
        {

            sgsReadSharedMemory(tempInfo[0], &dLog);
            cJSON_AddItemToArray(tempArray, tempObj = cJSON_CreateObject());
            cJSON_AddNumberToObject(tempObj, "Temp", dLog.value.i);
            cJSON_AddNumberToObject(tempObj, "Temp_Status", dLog.status);
            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf) - 1, "%02d",i++);
            cJSON_AddStringToObject(tempObj, "Temp_ID", buf);

        }
        else if(!strcmp(tempInfo[0]->valueName, "Irradiation"))
        {

            sgsReadSharedMemory(tempInfo[0], &dLog);
            irrVal[irrNum] = dLog.value.i;
            irrStat[irrNum] = dLog.status;
            irrNum++;

        }
        tempInfo[0] = tempInfo[0]->next;

    }

    //Second, Get inverter info & form JSON

    //Init JSON

    root = cJSON_CreateObject();

    //create rows, inverter obj and do the first time insert

    cJSON_AddItemToObject(root, "rows", rows = cJSON_CreateArray());

    inverter = cJSON_CreateObject();

    cJSON_AddItemToArray(rows, inverter);

    time(&nowTime);

    cJSON_AddNumberToObject(inverter, "Timestamp", nowTime);
    cJSON_AddStringToObject(inverter, "MAC_Address", postConfig.MAC_Address);
    cJSON_AddStringToObject(inverter, "GW_ID", postConfig.GW_ID);
    cJSON_AddNumberToObject(inverter, "CPU", systemInfo.cpuUsage);
    cJSON_AddNumberToObject(inverter, "Storage", systemInfo.storageUsage);
    cJSON_AddNumberToObject(inverter, "Memory", systemInfo.memoryUsage);
    cJSON_AddNumberToObject(inverter, "Network_flow", systemInfo.networkFlow);


    while(tempInfo[0] != NULL)
    {

        sgsReadSharedMemory(tempInfo[0], &dLog);

        //If we meet Alarm, put PV_Temp and Irr into inverter obj then create a new object

        if(strcmp(tempInfo[0]->valueName, "Alarm"))
        {

            //If it's not the first time, add the temp and irr into it

            cJSON_AddItemToObject(inverter, "PV_Temp", tempArray);
            for(i = 0 ; i < irrNum ; i++)
            {

                cJSON_AddNumberToObject(inverter, "Irr", irrVal[i]);
                cJSON_AddNumberToObject(inverter, "Irr_Status", irrStat[i]);

            }
            cJSON_AddStringToObject(inverter, "Alarm", dLog.value.s);
            cJSON_AddStringToObject(inverter, "Station_ID", postConfig.Station_ID);

            //Create New inverter obj

            inverter = cJSON_CreateObject();
            cJSON_AddItemToArray(rows, inverter);

            cJSON_AddNumberToObject(inverter, "Timestamp", nowTime);
            cJSON_AddStringToObject(inverter, "MAC_Address", postConfig.MAC_Address);
            cJSON_AddStringToObject(inverter, "GW_ID", postConfig.GW_ID);
            cJSON_AddNumberToObject(inverter, "CPU", systemInfo.cpuUsage);
            cJSON_AddNumberToObject(inverter, "Storage", systemInfo.storageUsage);
            cJSON_AddNumberToObject(inverter, "Memory", systemInfo.memoryUsage);
            cJSON_AddNumberToObject(inverter, "Network_flow", systemInfo.networkFlow);

        }
        else
        {

            if(dLog.valueType == INTEGER_VALUE)
            cJSON_AddNumberToObject(inverter, tempInfo[0]->valueName, dLog.value.i);

            if(dLog.valueType == LONGLONG_VALUE)
            {

                memset(buf,0,sizeof(buf));
                snprintf(buf,255,"%lld",dLog.value.ll);
                cJSON_AddStringToObject(inverter, tempInfo[0]->valueName, buf);

            }
            

        }

    }

    //3. Post them

    jsonBuff = cJSON_PrintUnformatted(root);

    i = 0;
    count = 10;

    while(i < 4)
    {

        ret = process_http(jsonBuff, postConfig.IP[i]);
        if(ret !=0 && count > 0)
        {
 
            count--;
            continue;

        }
        else
        {

            if(count == 0)
            {

                memset(buf,0,sizeof(buf));
                snprintf(buf,sizeof(buf) -1, "%s;upload to %s failed",ERROR, postConfig.IP[i]);
                sgsSendQueueMsg(eventHandlerId, buf, msgId);

            }
            i++;
            count = 10;

        }
        

    }

    free(jsonBuff);
    cJSON_Delete(root);

    return 0;

}

static int callback(void *data, int argc, char **argv, char **azColName)
{

    int i, tempID = 1, firstTempArray = 1, inverterID = 1;
    char *unformatted = NULL;
    char buf[32] = {0};
    epochTime nowTime;
    cJSON *inverter = NULL;
    cJSON *tempArray = NULL;
    cJSON *outerObj = NULL;

    time(&nowTime);

    fprintf(stderr, "Callback start: \n");

    //Fill first json object here

    outerObj = cJSON_CreateObject();

    cJSON_AddItemToObject(outerObj, "rows", jsonRoot = cJSON_CreateArray());

    cJSON_AddItemToArray(jsonRoot, inverter = cJSON_CreateObject());

    cJSON_AddNumberToObject(inverter, "Timestamp", nowTime);
    cJSON_AddStringToObject(inverter, "GW_ID", postConfig.GW_ID);
    cJSON_AddStringToObject(inverter, "MAC_Address", postConfig.MAC_Address);
    cJSON_AddStringToObject(inverter, "Station_ID", postConfig.Station_ID);
    snprintf(buf, 31, "%02d", inverterID);
    cJSON_AddStringToObject(inverter, "InverterID", buf);//insert inverterID (count manually by inverterID)

    //fill up the rest of the data with callback datas

    for(i=0; i<argc; i++)
    {

        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");

        //Make json object here

        if(strstr(azColName[i], "Inverter_Status") ) //Create a new object after this
        {

            cJSON_AddStringToObject(inverter, "Inverter_Status", argv[i]);

            if( i != argc -1)
            {

                cJSON_AddItemToArray(jsonRoot, inverter = cJSON_CreateObject());
                cJSON_AddNumberToObject(inverter, "Timestamp", nowTime);
                cJSON_AddStringToObject(inverter, "GW_ID", postConfig.GW_ID);
                cJSON_AddStringToObject(inverter, "MAC_Address", postConfig.MAC_Address);
                cJSON_AddStringToObject(inverter, "Station_ID", postConfig.Station_ID);
                memset(buf, 0, sizeof(buf));
                inverterID++;                           // Next ID
                tempID = 1;
                snprintf(buf, 31, "%02d", inverterID);
                cJSON_AddStringToObject(inverter, "InverterID", buf);

            }

        }
        else    //keep filling in current object
        {

            if(strstr(azColName[i], "sensorName"))
            {
                printf("Skipping sensorName \n");
            }
            else if(strstr(azColName[i], "Timestamp"))
            {

                cJSON_AddStringToObject(inverter, "upload_timestamp", argv[i]);

            }
            else if(strstr(azColName[i], "CPU_Usage"))
            {

                cJSON_AddStringToObject(inverter, "CPU", argv[i]);

            }
            else if(strstr(azColName[i], "Storage_Usage"))
            {

                cJSON_AddStringToObject(inverter, "Storage", argv[i]);

            }
            else if(strstr(azColName[i], "Memory_Usage"))
            {

                cJSON_AddStringToObject(inverter, "Memory", argv[i]);

            }
            else if(strstr(azColName[i], "Network_Flow"))
            {

                cJSON_AddStringToObject(inverter, "Network_flow", argv[i]);

            }
            else if(strstr(azColName[i], "Irradiation"))
            {

                cJSON_AddStringToObject(inverter, "Irr", argv[i]);

            }
            else if(strstr(azColName[i], "IrrStatus"))
            {

                cJSON_AddStringToObject(inverter, "Irr_Status", argv[i]);

            }
            else if(strstr(azColName[i], "Temperature"))
            {

                cJSON_AddStringToObject(inverter, "Inverter_Temp", argv[i]);

            }
            else if(strstr(azColName[i], "Voltage(Vab)"))
            {

                cJSON_AddStringToObject(inverter, "L1_ACV", argv[i]);

            }
            else if(strstr(azColName[i], "CurrentA"))
            {

                cJSON_AddStringToObject(inverter, "L1_ACA", argv[i]);

            }
            else if(strstr(azColName[i], "WattageA"))
            {

                cJSON_AddStringToObject(inverter, "L1_ACP", argv[i]);

            }
            else if(strstr(azColName[i], "FrequencyA"))
            {

                cJSON_AddStringToObject(inverter, "L1_AC_Freq", argv[i]);

            }
            else if(strstr(azColName[i], "Voltage(Vbc)"))
            {

                cJSON_AddStringToObject(inverter, "L2_ACV", argv[i]);

            }
            else if(strstr(azColName[i], "CurrentB"))
            {

                cJSON_AddStringToObject(inverter, "L2_ACA", argv[i]);

            }
            else if(strstr(azColName[i], "WattageB"))
            {

                cJSON_AddStringToObject(inverter, "L2_ACP", argv[i]);

            }
            else if(strstr(azColName[i], "FrequencyB"))
            {

                cJSON_AddStringToObject(inverter, "L2_AC_Freq", argv[i]);

            }
            else if(strstr(azColName[i], "Voltage(Vca)"))
            {

                cJSON_AddStringToObject(inverter, "L3_ACV", argv[i]);

            }
            else if(strstr(azColName[i], "CurrentC"))
            {

                cJSON_AddStringToObject(inverter, "L3_ACA", argv[i]);

            }
            else if(strstr(azColName[i], "WattageC"))
            {

                cJSON_AddStringToObject(inverter, "L3_ACP", argv[i]);

            }
            else if(strstr(azColName[i], "FrequencyC"))
            {

                cJSON_AddStringToObject(inverter, "L3_AC_Freq", argv[i]);

            }
            else if(strstr(azColName[i], "VoltageDA"))
            {

                cJSON_AddStringToObject(inverter, "DC1_DCV", argv[i]);

            }
            else if(strstr(azColName[i], "CurrentDA"))
            {

                cJSON_AddStringToObject(inverter, "DC1_DCA", argv[i]);

            }
            else if(strstr(azColName[i], "WattageDA"))
            {

                cJSON_AddStringToObject(inverter, "DC1_DCP", argv[i]);

            }
            else if(strstr(azColName[i], "VoltageDB"))
            {

                cJSON_AddStringToObject(inverter, "DC2_DCV", argv[i]);

            }
            else if(strstr(azColName[i], "CurrentDB"))
            {

                cJSON_AddStringToObject(inverter, "DC2_DCA", argv[i]);

            }
            else if(strstr(azColName[i], "WattageDB"))
            {

                cJSON_AddStringToObject(inverter, "DC2_DCP", argv[i]);

            }
            else if(strstr(azColName[i], "Today_Wh"))
            {

                cJSON_AddStringToObject(inverter, "ACP_Daily", argv[i]);

            }
            else if(strstr(azColName[i], "Life_Wh"))
            {

                cJSON_AddStringToObject(inverter, "ACP_Life", argv[i]);

            }
            else if(strstr(azColName[i], "Inverter_Temp"))
            {

                cJSON *obj = NULL;

                if(firstTempArray)
                {

                    firstTempArray = 0;
                    cJSON_AddItemToObject(inverter, "PVTemp",tempArray = cJSON_CreateArray());

                }
                
                memset(buf,0,sizeof(buf));
                snprintf(buf, 31, "%02d", tempID++);
                cJSON_AddItemToArray(tempArray, obj = cJSON_CreateObject());
                cJSON_AddStringToObject(obj, "Temp", argv[i]);
                cJSON_AddStringToObject(obj, "Temp_Status", "1");
                cJSON_AddStringToObject(obj, "Temp_ID", buf);

            }
            else if(strstr(azColName[i], "Inverter_Error"))
            {

                cJSON_AddStringToObject(inverter, "Alarm", argv[i]);

            }


        }
        
    }

    jsonRoot = outerObj;

    printf(LIGHT_RED"Callback function end\n"NONE);
    return 0;

}

ssize_t process_http( char *content, char *address)
{
    
    int sockfd;
	struct sockaddr_in servaddr;
	char **pptr;
	char str[50];
	struct hostent *hptr;
	char sendline[MAXLINE + 1], recvline[MAXLINE + 1];
    int i = 0, ret = 0;
    char *error = NULL;
    char *hname = NULL;             //IP
    char *serverPort = NULL;        //port
    char page[128] = {'\0'};        //rest api
    char adrBuf[128] = {'\0'};
    char *tmp = NULL;
	ssize_t n;
    cJSON *root = NULL;
    cJSON *obj = NULL;

    //process address (host name, page, port) example, 203.73.24.133:8000/solar_rawdata

    strncpy(adrBuf, address, sizeof(adrBuf));

    hname = strtok(adrBuf, ":");
    serverPort = strtok(NULL, "/");
    tmp = strtok(NULL, "/");

    //Get what's behind the / 

    memset(page, 0, sizeof(page));
    snprintf(page, sizeof(page), "/%s",tmp);

    //Intialize host entity with server ip address

    if ((hptr = gethostbyname(hname)) == NULL) 
    {

		printf("[%s:%d] gethostbyname error for host: %s: %s", __FUNCTION__, __LINE__,hname ,hstrerror(h_errno));

		return -1;

	}

	printf("[%s:%d] hostname: %s\n",__FUNCTION__,__LINE__, hptr->h_name);

    //Set up address type (FAMILY)

	if (hptr->h_addrtype == AF_INET && (pptr = hptr->h_addr_list) != NULL) 
    {

		printf("[%s:%d] address: %s\n",__FUNCTION__,__LINE__,inet_ntop( hptr->h_addrtype , *pptr , str , sizeof(str) ));

	} 
    else
    {

		printf("[%s:%d] Error call inet_ntop \n",__FUNCTION__,__LINE__);

        return -1;

	}

    //Create socket

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd == -1)
    {

        printf("socket failed, errno %d, %s\n",errno,strerror(errno));
        return -1;

    }
    else
    {

        printf("[%s,%d]socket() done\n",__FUNCTION__,__LINE__);

    }

    //Set to 0  (Initialization)

	bzero(&servaddr, sizeof(servaddr));

    //Fill in the parameters

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(serverPort));
	inet_pton(AF_INET, str, &servaddr.sin_addr);

    //Connect to the target server

	ret = connect(sockfd, (SA *) & servaddr, sizeof(servaddr));

    if(ret == -1)
    {

        printf("connect() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    else
    {

        printf("[%s,%d]connect() done\n",__FUNCTION__,__LINE__);

    }

    //Header content for HTTP POST

	snprintf(sendline, MAXSUB,
		 "POST %s HTTP/1.1\r\n"
		 "Host: %s\r\n"
		 "Content-type: application/json; charset=UTF-8\r\n"
         "User-Agent: Kelier/0.1\r\n"
		 "Content-Length: %lu\r\n\r\n"
		 "%s", page, hname, strlen(content), content);

    //print out the content

    printf("sendline : \n %s\n",sendline);

    //Send the packet to the server

    ret = my_write(sockfd, sendline, strlen(sendline));

    if(ret == -1)
    {

        printf("write() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    printf("[%s,%d]Write() done\n",__FUNCTION__,__LINE__);

    //Get the result

    memset(recvline, 0, sizeof(recvline));

    ret = my_read(sockfd, recvline, (sizeof(recvline) - 1));

    if(ret == -1)
    {

        printf("read() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    printf("[%s,%d]Read done\n",__FUNCTION__,__LINE__);

    //Check the result with cJSON

    root = cJSON_Parse(recvline);

    obj = cJSON_GetObjectItem(root, "Upload_data");

    if( obj != NULL)
    {

        if(strcmp(obj->valuestring,"True")) //upload unsuccessfully
        {
            return -2; //tell PostToServer() to resend the data
        }
        else
        {

            obj = cJSON_GetObjectItem(root, "Config_flag");
            if( obj != NULL)
            {

                if(strcmp(obj->valuestring,"True")) //Set config
                {

                    cJSON_Delete(root);
                    return 0;

                }
                else
                {
                    cJSON_Delete(root);
                    return 0;
                }

            }

        }

    }
    //close the socket

    close(sockfd);

	return n;

}

int my_write(int fd, void *buffer, int length)
{

    int bytes_left;
    int written_bytes;
    char *ptr;

    ptr=buffer;
    bytes_left=length;

    while(bytes_left>0)
    {
            
            //printf("Write loop\n");
            written_bytes=write(fd,ptr,bytes_left);
            //printf("Write %d bytes\n",written_bytes);

            if(written_bytes<=0)
            {       

                if(errno==EINTR)

                    written_bytes=0;

                else             

                    return(-1);

            }

            bytes_left-=written_bytes;
            ptr+=written_bytes;   

    }

    return(0);

}

int my_read(int fd, void *buffer, int length)
{

    int bytes_left;
    int bytes_read;
    char *ptr;
    
    ptr=buffer;
    bytes_left=length;

    while(bytes_left>0)
    {

        bytes_read=read(fd,ptr,bytes_read);

        if(bytes_read<0)
        {

            if(errno==EINTR)

                bytes_read=0;

            else

                return(-1);

        }

        else if(bytes_read==0)
            break;

        bytes_left-=bytes_read;
        ptr+=bytes_read;

    }

    return(length-bytes_left);

}
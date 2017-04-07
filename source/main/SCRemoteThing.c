/*
    Name: Xu Xi-Ping
    Date: March 31,2017
    Last Update: March 31,2017
    Program statement: 
        This program uploads SC data to the tw server

*/

#include "twOSPort.h"
#include "twLogger.h"
#include "twApi.h"
#include "./source/ipcs/SGSipcs.h"
#include "./source/controlling/SGScontrol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/syslog.h>

/* Name of our thing */
char * thingName = "AAEON_test";

/* Server Details */
#define TW_HOST "140.118.70.136"
#define TW_APP_KEY "5330fda1-2f5d-4f15-8d63-56090d2efee3"

deviceInfo *deviceInfoPtr = NULL;
dataInfo *dataInfoPtr = NULL;

int shmid = 0;//Debug Only

/*****************
A simple structure to handle
properties. Not related to
the API in anyway, just for
the demo application.
******************/
struct  
{

	double TotalFlow;
	char FaultStatus;
	char InletValve;
	double Pressure;
	double Temperature;
	double TemperatureLimit;
	twLocation Location;
	char * BigGiantString;

} properties;


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


void shutdownTask(DATETIME now, void * params) 
{

	TW_LOG(TW_FORCE,"shutdownTask - Shutdown service called.  SYSTEM IS SHUTTING DOWN");
	twApi_UnbindThing(thingName);
	twSleepMsec(100);
	twApi_Delete();
	twLogger_Delete();
	exit(0);	
	
}

/***************
Data Collection Task
****************/

//Intent : upload data with a certain period of time
//Pre : the time when the function being called and void pointer for other datas
//Post : Nothing


#define DATA_COLLECTION_RATE_MSEC 10000
void dataCollectionTask(DATETIME now, void * params) 
{

	//tw ADT

	twDataShape *ds = NULL;
	twInfoTable *cpm_data = NULL;
    twInfoTableRow *row = NULL;

	//my ADT

	deviceInfo *target = deviceInfoPtr;
	deviceInfo *interface = NULL;
	dataInfo *dataPtr = NULL;
	dataInfo *tmp = NULL;
	dataLog dLog;

	int ret = 0;
	char snrName[32]; // sensorName
	char now_time[32];

	printf("[%s,%d] dataCollectionTask starts\n",__FUNCTION__,__LINE__);

    while(target != NULL)
	{

		if(strcmp(target->deviceName,"cpm70_agent"))
			target = target->next;
		else
		{

			dataPtr = target->dataInfoPtr;
			break;

		}

	}
	if(dataPtr == NULL)
	{

		printf("No target available, bye bye\n");

		forceQuit(0);

	}

	target = deviceInfoPtr;

	while(target != NULL)
	{

		if(strcmp(target->deviceName,"SCRemoteThing"))
			target = target->next;
		else
		{

			interface = target;
			break;

		}

	}
	if(interface == NULL)
	{

		printf("No interface available, bye bye\n");
		forceQuit(0);

	}

	//Get time string for Creating datashape

	twGetTimeString(now, now_time, "%04Y%02m%02d%02H%02M%02S", 31, 0, TRUE);

	//An Entry notice tw that we update the data

	ds = twDataShape_Create(twDataShapeEntry_Create(now_time,NULL,TW_DATETIME));
    if (ds == NULL) 
	{
        return ;
    }

	//Done the rest Entry

	tmp = dataPtr;

	if(tmp == NULL)
	{

		printf("Can't get anything, bye\n");
		return ;

	}
	else
	{

		memset(snrName,'\0',sizeof(snrName));
		strncpy(snrName,tmp->sensorName,sizeof(snrName));

	}

	//printf("%s and %s\n",tmp->sensorName,snrName);

	while(!strcmp(tmp->sensorName,snrName))
	{
		
		twDataShape_AddEntry(ds, twDataShapeEntry_Create(tmp->valueName,NULL,TW_STRING));
		//printf("create:indata : %s record : %s value : %s\n",snrName,tmp->sensorName,tmp->valueName);//Debug
		tmp = tmp->next;

	}

	//printf("ds done\n");

	//Create infotable

	cpm_data =  twInfoTable_Create(ds);

	//Reset tmp to head of dataInfoPtr

	tmp = dataPtr;

	//Clear the snrName, we'll use it later

	memset(snrName,'\0',sizeof(snrName));

	dLog.valueType = STRING_VALUE;

	while(tmp != NULL)
	{

		if(strcmp(tmp->sensorName,snrName))
		{

			if(row != NULL)
				twInfoTable_AddRow(cpm_data, row);

			row = twInfoTableRow_Create(twPrimitive_CreateFromDatetime(now));

			//Update the snrName for comparison

			strncpy(snrName,tmp->sensorName,sizeof(snrName));

			memset(dLog.value.s,'\0',sizeof(dLog.value.s));

			sgsReadSharedMemory(tmp,&dLog);
			//printf("if:indata : %s record : %s value : %s\n",snrName,tmp->sensorName,dLog.value.s);//Debug
			twInfoTableRow_AddEntry(row, twPrimitive_CreateFromString(dLog.value.s, TRUE));
			tmp = tmp->next;

		}
		else
		{

			sgsReadSharedMemory(tmp,&dLog);
			twInfoTableRow_AddEntry(row, twPrimitive_CreateFromString(dLog.value.s, TRUE));
			//printf("else:indata : %s record : %s value : %s\n",snrName,tmp->sensorName,dLog.value.s);//Debug
			tmp = tmp->next;

		}
		
	}

	if(row != NULL)
				twInfoTable_AddRow(cpm_data, row);

	//Invoke upload function

	ret = twApi_WriteProperty(TW_THING, thingName, "cpm_data", twPrimitive_CreateFromInfoTable(cpm_data), -1, FALSE);

	if(ret != 0)
	{
	
		printf("Upload failed, return code %d \n",ret);

	}
	else
	{

		printf("Upload successfully\n");

	}
	return;

}

//Intent : Check connection status between Remotething and TW. If it's disconnected, the function will reconnect. 
//Pre : the time when the function being called and void pointer for other datas
//Post : Nothing

#define CheckPeriod 60000
void CheckandReconnect() 
{

	int ret;

	//printf("CHecking connection\n");

	ret = twApi_isConnected();

	if (ret == 1) 
	{

		//printf("Gateway Status : True\n");

		syslog(LOG_INFO, "[%s:%d] Gateway Status : True ",__FUNCTION__, __LINE__);
	
	}
	else 
	{

		//printf("Gateway Status : False\n");

		syslog(LOG_INFO, "[%s:%d] Gateway Status : False ",__FUNCTION__, __LINE__);

		/* Connect to server */

		if ((ret = twApi_Connect(CONNECT_TIMEOUT, twcfg.connect_retries)) != TW_OK) 
		{

			syslog(LOG_INFO, "[%s:%d] Gateway reconnect to Server failed",__FUNCTION__, __LINE__);

			//printf("Gateway Connect to Server Failed\n");

			return;

		}
		else
		{

			syslog(LOG_INFO, "[%s:%d] Gateway reconnect to Server Successfully",__FUNCTION__, __LINE__);

			//printf("Gateway Connect to Server Successfully\n");

		}		
	}
}

enum msgCodeEnum propertyHandler(const char * entityName, const char * propertyName,  twInfoTable ** value, char isWrite, void * userdata) 
{

	char * asterisk = "*";
	if (!propertyName) propertyName = asterisk;
	TW_LOG(TW_TRACE,"propertyHandler - Function called for Entity %s, Property %s", entityName, propertyName);
	if (value) {
		if (isWrite && *value) {
			/* Property Writes */
			if (strcmp(propertyName, "InletValve") == 0) twInfoTable_GetBoolean(*value, propertyName, 0, &properties.InletValve); 
			else if (strcmp(propertyName, "FaultStatus") == 0) twInfoTable_GetBoolean(*value, propertyName, 0, &properties.FaultStatus);
			else if (strcmp(propertyName, "TemperatureLimit") == 0) twInfoTable_GetNumber(*value, propertyName, 0, &properties.TemperatureLimit);
			else if (strcmp(propertyName, "BigGiantString") == 0) {
				char * bigString;
				twInfoTable_GetString(*value, propertyName, 0, &bigString);
				TW_LOG(TW_INFO,"Got bigString. Value:\n%s", bigString);
				TW_FREE(bigString);
			}
			else return TWX_NOT_FOUND;
			return TWX_SUCCESS;
		} else {
			/* Property Reads */
			if (strcmp(propertyName, "InletValve") == 0) *value = twInfoTable_CreateFromBoolean(propertyName, properties.InletValve); 
			else if (strcmp(propertyName, "FaultStatus") == 0) *value = twInfoTable_CreateFromBoolean(propertyName, properties.FaultStatus); 
			else if (strcmp(propertyName, "TotalFlow") == 0) *value = twInfoTable_CreateFromNumber(propertyName, properties.TotalFlow); 
			else if (strcmp(propertyName, "Pressure") == 0) *value = twInfoTable_CreateFromNumber(propertyName, properties.Pressure); 
			else if (strcmp(propertyName, "Temperature") == 0) *value = twInfoTable_CreateFromNumber(propertyName, properties.Temperature); 
			else if (strcmp(propertyName, "TemperatureLimit") == 0) *value = twInfoTable_CreateFromNumber(propertyName, properties.TemperatureLimit); 
			else if (strcmp(propertyName, "Location") == 0) *value = twInfoTable_CreateFromLocation(propertyName, &properties.Location); 
			else if (strcmp(propertyName, "BigGiantString") == 0) *value = twInfoTable_CreateFromString(propertyName, properties.BigGiantString, TRUE);
			else return TWX_NOT_FOUND;
		}
		return TWX_SUCCESS;
	} else {
		TW_LOG(TW_ERROR,"propertyHandler - NULL pointer for value");
		return TWX_BAD_REQUEST;
	}

}

/*****************
Service Callbacks 
******************/
/* Example of handling a single service in a callback */
enum msgCodeEnum addNumbersService(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) 
{

	double a, b, res;
	TW_LOG(TW_TRACE,"addNumbersService - Function called");
	if (!params || !content) {
		TW_LOG(TW_ERROR,"addNumbersService - NULL params or content pointer");
		return TWX_BAD_REQUEST;
	}

	twInfoTable_GetNumber(params, "a", 0, &a);
	twInfoTable_GetNumber(params, "b", 0, &b);
	res = a + b;
	*content = twInfoTable_CreateFromNumber("result", res);
	if (*content) return TWX_SUCCESS;
	else return TWX_INTERNAL_SERVER_ERROR;

}

/*!
 * \brief Example function to output an InfoTable containing arbitrary SteamSensor data (see #service_cb).
*/
enum msgCodeEnum getSteamSensorReadingsService(const char *entityName, const char *serviceName, twInfoTable *params, twInfoTable **content, void *userdata) 
{

    /* define pointers for use later on */ 
    twInfoTable *it = NULL; /* InfoTable pointer -- this will be our output */
    twDataShape *ds = NULL; /* DataShape pointer -- we will create the output InfoTable from this DataShape */
    twInfoTableRow *row = NULL; /* InfoTableRow pointer -- we will use this pointer to add data to the InfoTable */

    /* error checking -- there is no input to this function therefore there is no reason to check the NULL params pointer */
    TW_LOG(TW_TRACE,"getSteamSensorReadingsService - Function called");
    if (!content) {
        TW_LOG(TW_ERROR, "getSteamSensorReadingsService - NULL content pointer");
        return TW_BAD_REQUEST;
    }
 
    /* create the output DataShape */
    ds = twDataShape_Create( twDataShapeEntry_Create("SensorName", NULL, TW_STRING)); 
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("Temperature", NULL, TW_NUMBER));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("Pressure", NULL, TW_NUMBER));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("FaultStatus", NULL, TW_BOOLEAN));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("InletValve", NULL, TW_BOOLEAN));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("TemperatureLimit", NULL, TW_NUMBER));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("TotalFlow", NULL, TW_INTEGER));

    /* create an output InfoTable from the DataShape */
    it = twInfoTable_Create(ds);
    if (!it) {
        TW_LOG(TW_ERROR, "getSteamSensorReadingsService - Error creating infotable");
        twDataShape_Delete(ds);
        return TWX_INTERNAL_SERVER_ERROR;
    }

    /* create an InfoTableRow for the first sensor */
    /* InfoTableRow entry data MUST be added to the row in the same order as the DataShape entry data (even if the entry is empty) */
    row = twInfoTableRow_Create(twPrimitive_CreateFromString("Sensor Alpha", TRUE)); /* create and seed the InfoTableRow with the first (SensorName) entry */
     if (!row) {
        TW_LOG(TW_ERROR, "getSteamSensorReadingsService - Error creating infotable row");
        twInfoTable_Delete(it);
        return TWX_INTERNAL_SERVER_ERROR;
    }
    /* populate the InfoTableRow with arbitrary data */
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(60));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(25));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromBoolean(TRUE));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromBoolean(TRUE));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(150));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(77));
    /* add the InfoTableRow to the InfoTable */
    twInfoTable_AddRow(it, row);

    /* create an InfoTableRow for the second sensor */
    /* InfoTableRow entry data MUST be added to the row in the same order as the DataShape entry data (even if the entry is empty) */
    row = twInfoTableRow_Create(twPrimitive_CreateFromString("Sensor Beta", TRUE)); /* create and seed the InfoTableRow with the first (SensorName) entry */
     if (!row) {
        TW_LOG(TW_ERROR, "getSteamSensorReadingsService - Error creating infotable row");
        twInfoTable_Delete(it);
        return TWX_INTERNAL_SERVER_ERROR;
    }
    /* populate the InfoTableRow with arbitrary data */
    /* this SteamSensor does not read temperature */
    twInfoTableRow_AddEntry(row, twPrimitive_Create()); /* empty Temperature row member of type TW_NOTHING */
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(35));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromBoolean(FALSE));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromBoolean(TRUE));
    twInfoTableRow_AddEntry(row, twPrimitive_Create()); /* empty TemperatureLimit row member of type TW_NOTHING */
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(88));
    /* add the InfoTableRow to the InfoTable */
    twInfoTable_AddRow(it, row);

    /* create an InfoTableRow for the third sensor */
    /* InfoTableRow entry data MUST be added to the row in the same order as the DataShape entry data (even if the entry is empty) */
    row = twInfoTableRow_Create(twPrimitive_CreateFromString("Sensor Gamma", TRUE)); /* create and seed the InfoTableRow with the first (SensorName) entry */
     if (!row) {
        TW_LOG(TW_ERROR, "getSteamSensorReadingsService - Error creating infotable row");
        twInfoTable_Delete(it);
        return TWX_INTERNAL_SERVER_ERROR;
    }
    /* populate the InfoTableRow with arbitrary data */
    /* this SteamSensor does not read pressure */
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(80));
    twInfoTableRow_AddEntry(row, twPrimitive_Create()); /* empty Pressure row member of type TW_NOTHING */
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromBoolean(TRUE));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromBoolean(FALSE));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(150));
    twInfoTableRow_AddEntry(row, twPrimitive_CreateFromNumber(99));
    /* add the InfoTableRow to the InfoTable */
    twInfoTable_AddRow(it, row);

    /* output is always returned as an InfoTable type, since our output is an InfoTable type we can just return it */
    *content = it;
    if (*content) return TWX_SUCCESS;
    else return TWX_INTERNAL_SERVER_ERROR;
}

/* Example of handling multiple services in a callback */
enum msgCodeEnum multiServiceHandler(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) 
{

	TW_LOG(TW_TRACE,"multiServiceHandler - Function called");
	if (!content) {
		TW_LOG(TW_ERROR,"multiServiceHandler - NULL content pointer");
		return TWX_BAD_REQUEST;
	}
	if (strcmp(entityName, thingName) == 0) {
		if (strcmp(serviceName, "GetBigString") == 0) {
			int len = 10000;
			char text[] = "inna gadda davida ";
			char * bigString = (char *)TW_CALLOC(len,1);
			int textlen = strlen(text);
			while (bigString && len > textlen) {
				strncat(bigString, text, len - textlen - 1);
				len = len - textlen;
			}
			*content = twInfoTable_CreateFromString("result", bigString, FALSE);
			if (*content) return TWX_SUCCESS;
			return TWX_ENTITY_TOO_LARGE;
		} else if (strcmp(serviceName, "Shutdown") == 0) {
			/* Create a task to handle the shutdown so we can respond gracefully */
			twApi_CreateTask(1, shutdownTask);
		}
		return TWX_NOT_FOUND;	
	}
	return TWX_NOT_FOUND;	

}

/*******************************************/
/*         Bind Event Callback             */
/*******************************************/
void BindEventHandler(char * entityName, char isBound, void * userdata) 
{
	if (isBound) TW_LOG(TW_FORCE,"BindEventHandler: Entity %s was Bound", entityName);
	else TW_LOG(TW_FORCE,"BindEventHandler: Entity %s was Unbound", entityName);
}

/*******************************************/
/*    OnAuthenticated Event Callback       */
/*******************************************/
void AuthEventHandler(char * credType, char * credValue, void * userdata) 
{
	if (!credType || !credValue) return;
	TW_LOG(TW_FORCE,"AuthEventHandler: Authenticated using %s = %s.  Userdata = 0x%x", credType, credValue, userdata);
	/* Could do a delayed bind here */
	/* twApi_BindThing(thingName); */
}

/***************
Main Loop
****************/
/*
Solely used to instantiate and configure the API.
*/

int main( int argc, char* argv[] ) 
{

	int16_t port = 8443;

	struct sigaction act, oldact;
	twDataShape * ds = 0;
	int err = 0;
	int ret = 0;

#ifndef ENABLE_TASKER

	DATETIME nextDataCollectionTime = 0;

#endif

	twLogger_SetLevel(TW_INFO);
	twLogger_SetIsVerbose(1);
	TW_LOG(TW_FORCE, "Starting up");

	/* Initialize Properties */

	properties.Location.longitude = 43.221462;
	properties.Location.latitude = -77.850917;

	sleep(10);


	openlog("SCRemoteThing", LOG_PID, LOG_USER);

	//Record our pid

	ret = sgsInitControl("SCRemoteThing");
    if(ret < 0)
    {

        printf("SCRemoteThing aborting\n");
        return -1;

    }

	//Here we get every data ptr ready

	ret = initializeInfo();
	if(ret <= 0)
	{

		printf("Failed to initialize configuration, aborting\n");
		exit(0);

	}

	shmid = ret;//Debug only


	act.sa_handler = (__sighandler_t)forceQuit;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

	/* Initialize the API */

	err = twApi_Initialize(TW_HOST, port, TW_URI, TW_APP_KEY, NULL, MESSAGE_CHUNK_SIZE, MESSAGE_CHUNK_SIZE, TRUE);

	if (err) 
	{

		TW_LOG(TW_ERROR, "Error initializing the API");
		exit(err);

	}



	/* Set up for connecting through an HTTP proxy: Auth modes supported - Basic, Digest and None */
	/****
	twApi_SetProxyInfo("10.128.0.90", 3128, "proxyuser123", "thingworx");
	****/

	/* Allow self signed certs */

	twApi_SetSelfSignedOk();

	/* Enable FIPS mode (Not supported by AxTLS, yuo MUST use OpenSSL compiled for FIPS mode) */
	/****
	err = twApi_EnableFipsMode();
	if (err) {
		TW_LOG(TW_ERROR, "Error enabling FIPS mode.  Error code: %d", err);
		exit(err);
	}
	****/

	/* Register our services that have inputs */
	/* Create DataShape */

	ds = twDataShape_Create(twDataShapeEntry_Create("a",NULL,TW_NUMBER));

	if (!ds) 
	{

		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);

	}

	twDataShape_AddEntry(ds, twDataShapeEntry_Create("b",NULL,TW_NUMBER));

	/* Register the service */

	twApi_RegisterService(TW_THING, thingName, "AddNumbers", NULL, ds, TW_NUMBER, NULL, addNumbersService, NULL);

	/* API now owns that datashape pointer, so we can reuse it */

	ds = NULL;

    /* Create a DataShape for the SteamSensorReadings service output */

    ds = twDataShape_Create( twDataShapeEntry_Create("SensorName", NULL, TW_STRING)); 

	if (!ds) 
	{

		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);

	}

    twDataShape_AddEntry(ds, twDataShapeEntry_Create("Temperature", NULL, TW_NUMBER));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("Pressure", NULL, TW_NUMBER));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("FaultStatus", NULL, TW_BOOLEAN));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("InletValve", NULL, TW_BOOLEAN));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("TemperatureLimit", NULL, TW_NUMBER));
    twDataShape_AddEntry(ds, twDataShapeEntry_Create("TotalFlow", NULL, TW_INTEGER));

    /* Name the DataShape for the SteamSensorReadings service output */

    twDataShape_SetName(ds, "SteamSensorReadings");

	/* Register the service */

	twApi_RegisterService(TW_THING, thingName, "GetSteamSensorReadings", NULL, NULL, TW_INFOTABLE, ds, getSteamSensorReadingsService, NULL);

	/* API now owns that datashape pointer, so we can reuse it */

	ds = NULL;

    /* Register our services that don't have inputs */

	twApi_RegisterService(TW_THING, thingName, "GetBigString", NULL, NULL, TW_STRING, NULL, multiServiceHandler, NULL);
	twApi_RegisterService(TW_THING, thingName, "Shutdown", NULL, NULL, TW_NOTHING, NULL, multiServiceHandler, NULL);

	/* Register our Events */

	/* Create DataShape */

	ds = twDataShape_Create(twDataShapeEntry_Create("message",NULL,TW_STRING));
	if (!ds) 
	{

		TW_LOG(TW_ERROR, "Error Creating datashape.");
		exit(1);

	}
	/* Event datashapes require a name */

	twDataShape_SetName(ds, "SteamSensor.Fault");

	/* Register the service */

	twApi_RegisterEvent(TW_THING, thingName, "SteamSensorFault", "Steam sensor event", ds);

	/* Regsiter our properties */

	twApi_RegisterProperty(TW_THING, thingName, "FaultStatus", TW_BOOLEAN, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "InletValve", TW_BOOLEAN, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "Pressure", TW_NUMBER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "Temperature", TW_NUMBER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "TemperatureLimit", TW_NUMBER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "TotalFlow", TW_NUMBER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "BigGiantString", TW_STRING, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "Location", TW_LOCATION, NULL, "ALWAYS", 0, propertyHandler, NULL);

	// Resgister our infotable property

    twApi_RegisterProperty(TW_THING,thingName,"cpm_data",TW_INFOTABLE,NULL,"ALWAYS",0,propertyHandler,NULL);

	/* Add any additoinal aspects to our properties */

	twApi_AddAspectToProperty(thingName, "TemperatureLimit", "defaultValue", twPrimitive_CreateFromNumber(50.5));

	/* Register an authentication event handler */

	twApi_RegisterOnAuthenticatedCallback(AuthEventHandler, NULL); /* Callbacks only when we have connected & authenticated */

	/* Register a bind event handler */

	twApi_RegisterBindEventCallback(thingName, BindEventHandler, NULL); /* Callbacks only when thingName is bound/unbound */

	/* twApi_RegisterBindEventCallback(NULL, BindEventHandler, NULL); *//* First NULL says "tell me about all things that are bound */
	
	/* Bind our thing - Can bind before connecting or do it when the onAuthenticated callback is made.  Either is acceptable */
	
	twApi_BindThing(thingName);

	/* Connect to server */

	err = twApi_Connect(CONNECT_TIMEOUT, twcfg.connect_retries);

	if (!err) 
	{

		/* Register our tasks with the tasker */

		twApi_CreateTask(DATA_COLLECTION_RATE_MSEC, dataCollectionTask);

		twApi_CreateTask(DATA_COLLECTION_RATE_MSEC, CheckandReconnect);

	} 
	else 
	{

		TW_LOG(TW_ERROR,"main: Server connection failed after %d attempts.  Error Code: %d", twcfg.connect_retries, err);

	}

	while(1) 
	{

#ifndef ENABLE_TASKER
		DATETIME now = twGetSystemTime(TRUE);
		twApi_TaskerFunction(now, NULL);
		twMessageHandler_msgHandlerTask(now, NULL);
		if (twTimeGreaterThan(now, nextDataCollectionTime)) 
		{

			dataCollectionTask(now, NULL);
			nextDataCollectionTime = twAddMilliseconds(now, DATA_COLLECTION_RATE_MSEC);

		}
#endif
		twSleepMsec(5);

	}

	twApi_UnbindThing(thingName);
	twSleepMsec(100);

	/* 
	twApi_Delete also cleans up all singletons including 
	twFileManager, twTunnelManager and twLogger. 
	*/

	twApi_Delete(); 
	exit(0);

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
    printf("[Ctrl + C] Catched (signal number %d) , forceQuitting...\n",sigNum);
    exit(0);

}

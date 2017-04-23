/*
 *  Copyright (C) 2014 ThingWorx Inc.
 *
 *  Test application
 */

#include "twOSPort.h"
#include "twLogger.h"
#include "twApi.h"
#include "twThreads.h"

#include <stdio.h>
#include <string.h>

/* Name of our thing */
char * thingName = "SteamSensor1";

/* Server Details */
#define TW_HOST "localhost"
#define TW_APP_KEY "1724be81-fa15-4485-a966-287bf8f6683c"

/* Declarations for our threads */
#define NUM_WORKERS 4
twList * workerThreads = NULL;
twThread * apiThread = NULL;
twThread * dataCollectionThread = NULL;
twThread * subscribedPropertyThread = NULL;

/*****************
A simple structure to handle
properties. Not related to
the API in anyway, just for
the demo application.
******************/
struct  {
	double TotalFlow;
	char FaultStatus;
	char InletValve;
	double Pressure;
	double Temperature;
	double TemperatureLimit;
	twLocation Location;
	char * BigGiantString;
} properties;

/*****************
Helper Functions
*****************/
void sendPropertyUpdate() {
	/* 
	This uses the SubscribedProperty function which follows the PUSH
	rules set forth in the property definition.  Each property can be pushed
	when written, or for much better bandwidth efficiency you can queue up
	several updates (even updates of the same type as long as they have different
	timestamps) and send them as a group.
	*/
	twPrimitive * value = 0;
	value = twPrimitive_CreateFromBoolean(properties.FaultStatus);
	twApi_SetSubscribedPropertyVTQ(thingName, "FaultStatus", value, twGetSystemTime(TRUE), "GOOD", FALSE, FALSE);
	value = twPrimitive_CreateFromBoolean(properties.InletValve);
	twApi_SetSubscribedPropertyVTQ(thingName, "InletValve", value, twGetSystemTime(TRUE), "GOOD", FALSE, FALSE);
	value = twPrimitive_CreateFromNumber(properties.Temperature);
	twApi_SetSubscribedPropertyVTQ(thingName, "Temperature", value, twGetSystemTime(TRUE), "GOOD", FALSE, FALSE);
	value = twPrimitive_CreateFromNumber(properties.TemperatureLimit);
	twApi_SetSubscribedPropertyVTQ(thingName, "TemperatureLimit", value, twGetSystemTime(TRUE), "GOOD", FALSE, FALSE);
	value = twPrimitive_CreateFromNumber(properties.TotalFlow);
	twApi_SetSubscribedPropertyVTQ(thingName, "TotalFlow", value, twGetSystemTime(TRUE), "GOOD", FALSE, FALSE);
	value = twPrimitive_CreateFromNumber(properties.Pressure);
	twApi_SetSubscribedPropertyVTQ(thingName, "Pressure", value, twGetSystemTime(TRUE), "GOOD", FALSE, FALSE);
	value = twPrimitive_CreateFromLocation(&properties.Location);
	twApi_SetSubscribedPropertyVTQ(thingName, "Location", value, twGetSystemTime(TRUE), "GOOD", FALSE, FALSE);
	/***  Push the update ***/
	twApi_PushSubscribedProperties(thingName, FALSE);
}

void shutdownTask(DATETIME now, void * params) {
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
/*
This function gets called at the rate defined in the task creation.  The SDK has 
a simple cooperative multitasker, so the function cannot infinitely loop.
Use of a task like this is optional and not required in a multithreaded
environment where this functonality could be provided in a separate thread.
*/
#define DATA_COLLECTION_RATE_MSEC 5000
void dataCollectionTask(DATETIME now, void * params) {
    /* TW_LOG(TW_TRACE,"dataCollectionTask: Executing"); */
	properties.TotalFlow = rand()/(RAND_MAX/10.0);
	properties.Pressure = 18 + rand()/(RAND_MAX/5.0);
	properties.Location.latitude = properties.Location.latitude + ((double)(rand() - RAND_MAX))/RAND_MAX/5;
	properties.Location.longitude = properties.Location.longitude + ((double)(rand() - RAND_MAX))/RAND_MAX/5;
/////////////////	properties.Temperature  = 400 + rand()/(RAND_MAX/40);
	properties.Temperature += 1.0;
	/* Check for a fault.  Only do something if we haven't already */
	if (properties.Temperature > properties.TemperatureLimit && properties.FaultStatus == FALSE) {
		twInfoTable * faultData = 0;
		char msg[140];
		properties.FaultStatus = TRUE;
		properties.InletValve = TRUE;
		sprintf(msg,"%s Temperature %2f exceeds threshold of %2f", 
			thingName, properties.Temperature, properties.TemperatureLimit);
		faultData = twInfoTable_CreateFromString("message", msg, TRUE);
		twApi_FireEvent(TW_THING, thingName, "SteamSensorFault", faultData, -1, TRUE);
		twInfoTable_Delete(faultData);
	}
	/* Update the properties on the server */
	sendPropertyUpdate();
}

/*****************
Property Handler Callbacks 
******************/
enum msgCodeEnum propertyHandler(const char * entityName, const char * propertyName,  twInfoTable ** value, char isWrite, void * userdata) {
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
enum msgCodeEnum addNumbersService(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) {
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

/* Example of handling multiple services in a callback */
enum msgCodeEnum multiServiceHandler(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) {
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

/***************
Main Loop
****************/
/*
Solely used to instantiate and configure the API.
*/
int main( int argc, char** argv ) {

#if defined NO_TLS
	int16_t port = 80;
#else
	int16_t port = 443;
#endif
	twDataShape * ds = 0;
	int err = 0;

#ifndef ENABLE_TASKER
	DATETIME nextDataCollectionTime = 0;
#endif

	twLogger_SetLevel(TW_TRACE);
	twLogger_SetIsVerbose(1);
	TW_LOG(TW_FORCE, "Starting up");

	/* Initialize Properties */
	properties.Location.longitude = 43.221462;
	properties.Location.latitude = -77.850917;

	/* Initialize the API */
	err = twApi_Initialize(TW_HOST, port, TW_URI, TW_APP_KEY, NULL, MESSAGE_CHUNK_SIZE, MESSAGE_CHUNK_SIZE, TRUE);
	if (err) {
		TW_LOG(TW_ERROR, "Error initializing the API");
		exit(err);
	}

	/* Set up for connecting through an HTTP proxy: Auth modes supported - Basic, Digest and None */
	/*
	twApi_SetProxyInfo("10.64.74.90", 3128, "proxyuser", "thingworx");
	*/

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

	/* Register our services */
	ds = twDataShape_Create(twDataShapeEntry_Create("a",NULL,TW_NUMBER));
	twDataShape_AddEntry(ds, twDataShapeEntry_Create("b",NULL,TW_NUMBER));
	twApi_RegisterService(TW_THING, thingName, "AddNumbers", NULL, ds, TW_NUMBER, NULL, addNumbersService, NULL);
	twApi_RegisterService(TW_THING, thingName, "GetBigString", NULL, NULL, TW_STRING, NULL, multiServiceHandler, NULL);
	twApi_RegisterService(TW_THING, thingName, "Shutdown", NULL, NULL, TW_NOTHING, NULL, multiServiceHandler, NULL);

	/* Regsiter our properties */
	twApi_RegisterProperty(TW_THING, thingName, "FaultStatus", TW_BOOLEAN, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "InletValve", TW_BOOLEAN, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "Pressure", TW_NUMBER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "Temperature", TW_NUMBER, NULL, "VALUE", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "TemperatureLimit", TW_NUMBER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "TotalFlow", TW_NUMBER, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "BigGiantString", TW_STRING, NULL, "ALWAYS", 0, propertyHandler, NULL);
	twApi_RegisterProperty(TW_THING, thingName, "Location", TW_LOCATION, NULL, "ALWAYS", 0, propertyHandler, NULL);

	/* Bind our thing */
	twApi_BindThing(thingName);

	/* Connect to server */
	err = twApi_Connect(CONNECT_TIMEOUT, twcfg.connect_retries);
	if (!err) {

	} else {
		TW_LOG(TW_ERROR,"main: Server connection failed after %d attempts.  Error Code: %d", twcfg.connect_retries, err);
	}

	/******************************************/
	/*           Create our threads           */
	/******************************************/
	/* Create and start our worker Threads */
	workerThreads = twList_Create(twThread_Delete);
	if (workerThreads) {
		int i = 0;
		for (i = 0; i < NUM_WORKERS; i++) {
			twThread * tmp = twThread_Create(twMessageHandler_msgHandlerTask, 5, NULL, TRUE);
			if (!tmp) {
				TW_LOG(TW_ERROR,"main: Error creating worker thread.");
				break;
			}
			twList_Add(workerThreads, tmp);
		}
	} else {
		TW_LOG(TW_ERROR,"main: Error creating worker thread list.");
	}
	/* Create and start a thread for the Api tasker function */
	apiThread = twThread_Create(twApi_TaskerFunction, 5, NULL, TRUE);
	/* Create and start a thread for the data collection function */
	dataCollectionThread = twThread_Create(dataCollectionTask, DATA_COLLECTION_RATE_MSEC, NULL, TRUE); 

	/* Check for any errors */
	if (!apiThread || !dataCollectionThread) {
		TW_LOG(TW_ERROR,"main: Error creating a required thread.");
	}
	while(1) {
		char in = 0;
		in = getch();
		if (in == 'q') break;
		else printf("\n");
		twSleepMsec(5);
	}
	twApi_UnbindThing(thingName);
	twSleepMsec(100);
	twList_Delete(workerThreads);
	twThread_Delete(dataCollectionThread);
	twThread_Delete(apiThread);
	twApi_Delete(); 
	exit(0);
}

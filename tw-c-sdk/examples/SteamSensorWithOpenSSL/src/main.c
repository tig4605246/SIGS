/*
 *  Copyright (C) 2014 ThingWorx Inc.
 *
 *  Test application
 */

#include "twOSPort.h"
#include "twLogger.h"
#include "twApi.h"
#include "twFileManager.h"
#include "twTunnelManager.h"

#include <stdio.h>
#include <string.h>

/* Name of our thing */
char * thingName = "SteamSensor1"; 

/* Server Details */
#define TW_HOST "localhost"
#define TW_APP_KEY "1724be81-fa15-4485-a966-287bf8f6683c"

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
	/* This can be done with all at once via a service, or individually */
	/*** Individually - Minimizes footprint/complexity ***/
	/*
	twPrimitive * value = 0;
	value = twPrimitive_CreateFromBoolean(properties.FaultStatus);
	twApi_WriteProperty(TW_THING, thingName, "FaultStatus", value, -1);
	twPrimitive_Delete(value);
	value = twPrimitive_CreateFromBoolean(properties.InletValve);
	twApi_WriteProperty(TW_THING, thingName, "InletValve", value, -1);
	twPrimitive_Delete(value);
	value = twPrimitive_CreateFromNumber(properties.Temperature);
	twApi_WriteProperty(TW_THING, thingName, "Temperature", value, -1);
	twPrimitive_Delete(value);
	value = twPrimitive_CreateFromNumber(properties.TemperatureLimit);
	twApi_WriteProperty(TW_THING, thingName, "TemperatureLimit", value, -1);
	twPrimitive_Delete(value);
	value = twPrimitive_CreateFromNumber(properties.TotalFlow);
	twApi_WriteProperty(TW_THING, thingName, "TotalFlow", value, -1);
	twPrimitive_Delete(value);
	value = twPrimitive_CreateFromNumber(properties.Pressure);
	twApi_WriteProperty(TW_THING, thingName, "Pressure", value, -1);
	twPrimitive_Delete(value);
	value = twPrimitive_CreateFromLocation(&properties.Location);
	twApi_WriteProperty(TW_THING, thingName, "Location", value, -1);
	twPrimitive_Delete(value);
	*/
	/***  All at once -> Minimizes Bandwidth ***/

	/* Create the property list */
	propertyList * proplist = twApi_CreatePropertyList("FaultStatus",twPrimitive_CreateFromBoolean(properties.FaultStatus), 0);
	if (!proplist) {
		TW_LOG(TW_ERROR,"sendPropertyUpdate: Error allocating property list");
		return;
	}
	twApi_AddPropertyToList(proplist,"InletValve",twPrimitive_CreateFromBoolean(properties.InletValve), 0);
	twApi_AddPropertyToList(proplist,"Temperature",twPrimitive_CreateFromNumber(properties.Temperature), 0);
	twApi_AddPropertyToList(proplist,"TemperatureLimit",twPrimitive_CreateFromNumber(properties.TemperatureLimit), 0);
	twApi_AddPropertyToList(proplist,"TotalFlow",twPrimitive_CreateFromNumber(properties.TotalFlow), 0);
	twApi_AddPropertyToList(proplist,"Pressure",twPrimitive_CreateFromNumber(properties.Pressure), 0);
	twApi_AddPropertyToList(proplist,"Location",twPrimitive_CreateFromLocation(&properties.Location), 0);
	twApi_PushProperties(TW_THING, thingName, proplist, -1, FALSE);
	twApi_DeletePropertyList(proplist);
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
#define DATA_COLLECTION_RATE_MSEC 2000
void dataCollectionTask(DATETIME now, void * params) {
    /* TW_LOG(TW_TRACE,"dataCollectionTask: Executing"); */
	properties.TotalFlow = rand()/(RAND_MAX/10.0);
	properties.Pressure = 18 + rand()/(RAND_MAX/5.0);
	properties.Location.latitude = properties.Location.latitude + ((double)(rand() - RAND_MAX))/RAND_MAX/5;
	properties.Location.longitude = properties.Location.longitude + ((double)(rand() - RAND_MAX))/RAND_MAX/5;
	properties.Temperature  = 400 + rand()/(RAND_MAX/40);
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

/*!
 * \brief Example function to output an InfoTable containing arbitrary SteamSensor data (see #service_cb).
*/
enum msgCodeEnum getSteamSensorReadingsService(const char *entityName, const char *serviceName, twInfoTable *params, twInfoTable **content, void *userdata) {

    /* define pointers for use later on */ 
    twInfoTable *it = NULL; /* InfoTable pointer -- this will be our output */
    twDataShape *ds = NULL; /* DataShape pointer -- we will create the output InfoTable from this DataShape */
    twInfoTableRow *row = NULL; /* InfoTableRow pointer -- we will use this pointer to add data to the InfoTable */

    /* error checking -- there is no input to this function therefore there is no reason to check the NULL params pointer */
    TW_LOG(TW_TRACE,"getSteamSensorReadingsService - Function called");
    if (!content) {
        TW_LOG(TW_ERROR, "getSteamSensorReadingsService - NULL content pointer");
        return TWX_BAD_REQUEST;
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

/*******************************************/
/*         Bind Event Callback             */
/*******************************************/
void BindEventHandler(char * entityName, char isBound, void * userdata) {
	if (isBound) TW_LOG(TW_FORCE,"BindEventHandler: Entity %s was Bound", entityName);
	else TW_LOG(TW_FORCE,"BindEventHandler: Entity %s was Unbound", entityName);
}

/*******************************************/
/*    OnAuthenticated Event Callback       */
/*******************************************/
void AuthEventHandler(char * credType, char * credValue, void * userdata) {
	if (!credType || !credValue) return;
	TW_LOG(TW_FORCE,"AuthEventHandler: Authenticated using %s = %s.  Userdata = 0x%x", credType, credValue, userdata);
	/* Could do a delayed bind here */
	/* twApi_BindThing(thingName); */
}

/*******************************************/
/*    FileTransfer Event Callback          */
/*******************************************/
void fileCallbackFunc (char fileRcvd, twFileTransferInfo * info, void * userdata) {
	char startTime[80];
	char endTime[80];
	if (!info) {
		TW_LOG(TW_ERROR,"fileCallbackFunc: Function called with NULL info");
	}
	twGetTimeString(info->startTime, startTime, "%Y-%m-%d %H:%M:%S", 80, 1, 1);
	twGetTimeString(info->endTime, endTime, "%Y-%m-%d %H:%M:%S", 80, 1, 1);
	TW_LOG(TW_AUDIT,"\n\n*****************\nFILE TRANSFER NOTIFICATION:\nSource: %s:%s/%s\nDestination: %s:%s/%s\nSize: %9.0f\nStartTime: %s\nEndTime: %s\nDuration: %d msec\nUser: %s\nState: %s\nMessage: %s\nTransfer ID: %s\n*****************\n",
		info->sourceRepository, info->sourcePath, info->sourceFile, info->targetRepository, info->targetPath, 
		info->targetFile, info->size, startTime, endTime, info->duration, info->user, info->state, info->message, info->transferId);
}

/*******************************************/
/*      Tunneling Event Callback           */
/*******************************************/
void tunnelCallbackFunc (char started, const char * tid, const char * thingName, const char * peerName,
						   const char * host, int16_t port, DATETIME startTime,  DATETIME endTime, 
						   uint64_t bytesSent, uint64_t bytesRcvd, const char * type, const char * msg, void * userdata) {
	char startString[100] = "UNKNOWN";
	char endString[100] = "Still in Progress";
	uint32_t duration = endTime ? endTime - startTime : 0;

	if (startTime) twGetTimeString(startTime, startString, "%Y-%m-%d %H:%M:%S",99, TRUE, FALSE);
	if (endTime) twGetTimeString(endTime, endString, "%Y-%m-%d %H:%M:%S",99, TRUE, FALSE);

	TW_LOG(TW_AUDIT,"\n\n*****************\nTUNNEL NOTIFICATION:\nID: %s\nThingName: %s\nState: %s\nTarget: %s:%d\nStartTime: %s\nEndTime: %s\nDuration: %d msec\nUser: %s\nBytes Sent: %llu\nBytes Rcvd: %llu\nMessage: %s\n*****************\n",
		tid ? tid : "UNKNOWN", thingName ? thingName : "UNKNOWN", started ? "STARTED" : "ENDED", host ? host : "UNKNOWN", port, 
		startString, endString, duration, peerName ? peerName : "Unknown", bytesSent, bytesRcvd, msg ? msg : "" );	
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
	/****
	twApi_SetProxyInfo("10.128.0.90", 3128, "proxyuser123", "thingworx");
	****/

	/* Allow self signed certs */
	twApi_SetSelfSignedOk();

	/* Enable FIPS mode (Not supported by AxTLS, you MUST use OpenSSL compiled for FIPS mode) */
	err = twApi_EnableFipsMode();
	if (err) {
		TW_LOG(TW_ERROR, "Error enabling FIPS mode.  Error code: %d", err);
		exit(err);
	}

	/*****
	twApi_DisableCertValidation();
	*****/

	/* Set up any certificate fields to validate - NULLs are acceptable and will not be checked */
	/*  twApi_SetX509Fields("MY_SERVER_CN", "MY_SERVER_ORG", "MY_SERVER_ORG_UNIT", "MY_CA_CN", "MY_CA_ORG", "MY_CA_ORG_UNIT"); */

	/* Register our services that have inputs */
	/* Create DataShape */
	ds = twDataShape_Create(twDataShapeEntry_Create("a",NULL,TW_NUMBER));
	if (!ds) {
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
	if (!ds) {
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
	if (!ds) {
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

	/* Bind our thing */
	twApi_BindThing(thingName);

	/******************************************/
	/*      File Transfer Initialization      */
	/******************************************/
	/* Initialize the FileTransfer Manager */
	twFileManager_Create();

	/* Create our virtual directories */
	twFileManager_AddVirtualDir(thingName, "tw", "/opt/thingworx");
	twFileManager_AddVirtualDir(thingName, "tw2", "/twFile_tmp");

	/* Register the file transfer callback function */
	twFileManager_RegisterFileCallback(fileCallbackFunc, NULL, FALSE, NULL);

	/******************************************/
	/*        Tunneling Initialization        */
	/******************************************/
	/* Tunnel Manager is automatically created if ENABLE_TUNNELING is defined */

	/* Register the tunnel callback function */
	twTunnelManager_RegisterTunnelCallback(tunnelCallbackFunc, NULL, NULL);

	/* Adjust connection settings if they are different from the API websocket */
	/**********
	twTunnelManager_UpdateTunnelServerInfo(TW_HOST,port,TW_APP_KEY);
	twTunnelManager_SetProxyInfo("10.128.0.90", 3128, "proxyuser123", "thingworx");
	**********/

	/* Allow self signed certs */
	/**********	
	twTunnelManager_SetSelfSignedOk(FALSE);
	twTunnelManager_DisableCertValidation(TRUE);
	***********/
	
	/* Set up any certificate fields to validate - NULLs are acceptable and will not be checked */
	/**********
	twTunnelManager_SetX509Fields("MY_CERT_CN", "MY_CERT_ORG", "MY_CERT_ORG_UNIT", "MY_CA_CN", "MY_CA_ORG", "MY_CA_ORG_UNIT"); 
	**********/

	/* Connect to server */
	err = twApi_Connect(CONNECT_TIMEOUT, twcfg.connect_retries);
	if (!err) {
		/* Register our "Data collection Task" with the tasker */
		twApi_CreateTask(DATA_COLLECTION_RATE_MSEC, dataCollectionTask);  
	} else {
		TW_LOG(TW_ERROR,"main: Server connection failed after %d attempts.  Error Code: %d", twcfg.connect_retries, err);
	}

	while(1) {
	/*while (twApi_isConnected()) {*/
		char in = 0;
#ifndef ENABLE_TASKER
		DATETIME now = twGetSystemTime(TRUE);
		twApi_TaskerFunction(now, NULL);
		twMessageHandler_msgHandlerTask(now, NULL);
		if (twTimeGreaterThan(now, nextDataCollectionTime)) {
			dataCollectionTask(now, NULL);
			nextDataCollectionTime = twAddMilliseconds(now, DATA_COLLECTION_RATE_MSEC);
		}
#else
		in = getch();
		if (in == 'q') break;
		else printf("\n");
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

/*
 *  Copyright (C) 2015 ThingWorx Inc.
 *
 *  Portable ThingWorx Tunneling Component
 */

#include "twTunnelManager.h"
#include "twLogger.h"
#include "twInfoTable.h"
#include "twDefinitions.h"
#include "twApi.h"
#include "stringUtils.h"
#include "twList.h"
#include "twInfoTable.h"
#include "twWebsocket.h"
#include "twTls.h"
#include "tomcrypt.h"
#include "cJSON.h"

/***************************************/
/*  Tunnel Information struct   */
/***************************************/
enum tunnelType {
	TCP_TUNNEL,
	UDP_TUNNEL,
	SERIAL_TUNNEL,
	FILE_TUNNEL
};

typedef struct twTunnel {
	char * thingName;
	char * peerName;
	char * host;
	uint16_t port;
	DATETIME startTime;
	DATETIME endTime;
	char markForClose;
	int16_t numConnects;
	char isActive;
	char firstPeerConnect;
	uint64_t bytesSent;
	uint64_t bytesRcvd;
	char * tid;
	int32_t chunksize;
	int32_t readTimeout;
	int32_t idleTimeout;
	DATETIME idleTime;
	char * type;
	enum tunnelType typeEnum;
	twWs * ws;
	void * localSocket;
	char * readBuffer;
	char * readBase64;
	char * writeBuffer;
	char disconnectedByPeer;
	char * msg;
} twTunnel;

/*
twTunnel_Create - Creates a tunnel info structure
Parameters:
    thingName - (Input) name of the thing the tunnel is associated with
	it - (Input) infotable that contains the params for the tunnel
Return:
	twTunnel * - pointer to the allocated structure, 0 if an error occurred
*/
twTunnel * twTunnel_Create(const char * thingName, twInfoTable * it);

/*
twTunnel_Delete - Delete the tunnel info structure
Parameters:
	tunnel - pointer to the structure to delete
Return:
	Nothing
*/
void twTunnel_Delete(void * tunnel);


/***************************************/
/* Singleton Tunnel Manager Structure  */
/***************************************/
typedef struct twTunnelManager {
	TW_MUTEX mtx;
	twList * openTunnels;
	twList * callbacks;
	twConnectionInfo * info;
} twTunnelManager;

twTunnelManager * tm = NULL;

/***************************************/
/*  Private Tunnel Manager Functions  */
/***************************************/
/*
twTunnelManager_AddTunnel - Registers a new tunnel with the manager
Parameters:
	tunnel - (Input) Pointer to a tunnel stucture that has the information required to run the tunnel.  
Return:
	int - 0 if successful, positive integral error code (see twErrors.h) if an error was encountered
*/
int twTunnelManager_AddTunnel(twTunnel * tunnel);

/*
twTunnelManager_DeleteWIthPointer - Delete the file manager singleton
Parameters:
	mgr - pointer to the tunnel manager.  If NULL then the tm singleton is used.
Return:
	int - 0 if successful, positive integral error code (see twErrors.h) if an error was encountered
*/
int twTunnelManager_DeleteWithPointer(twTunnelManager * mgr);

/***************************************/
/*           Helper Functions          */
/***************************************/
twTunnel * getTunnelFromWebsocket(twWs * ws) {
	ListEntry * le = NULL;
	twTunnel * t = NULL;
	if (!ws || !tm || !tm->openTunnels) {
		TW_LOG(TW_ERROR, "getTunnelFromWebsocket: NULL input parameter");
		return NULL;
	}
#ifdef ENABLE_TASKER
	/* Don't lock here since this will be called from something that already has the lock */
	twMutex_Unlock(tm->mtx); 
#endif
	le = twList_Next(tm->openTunnels, NULL);
	while (le && le->value) {
		t = (twTunnel *) le->value;
		if (t->ws == ws) break;
		t = NULL;
		le = twList_Next(tm->openTunnels, le);
	}
#ifdef ENABLE_TASKER
	twMutex_Unlock(tm->mtx);
#endif
	return t;
}

twTunnel * getTunnelFromId(char * id, ListEntry ** entry) {
	ListEntry * le = NULL;
	twTunnel * t = NULL;
	if (!id || !tm || !tm->openTunnels) {
		TW_LOG(TW_ERROR, "getTunnelFromId: NULL input parameter");
		return NULL;
	}
#ifdef ENABLE_TASKER
	/* Don't lock here since this will be called from something that already has the lock */
/*	twMutex_Lock(tm->mtx); */
#endif
	le = twList_Next(tm->openTunnels, NULL);
	while (le && le->value) {
		t = (twTunnel *) le->value;
		if (t->tid && !strcmp(t->tid, id)) break;
		t = NULL;
		le = twList_Next(tm->openTunnels, le);
	}
	if (entry) *entry = le;
#ifdef ENABLE_TASKER
/*	twMutex_Unlock(tm->mtx); */
#endif
	return t;
}

int sendTunnelCommand(char * thingName, const char * tid, const char * msg) {
	twDataShape * ds = NULL;
	if (!msg || !thingName || !tid) {
		TW_LOG(TW_ERROR, "sendTunnelCommand: NULL message pointer passed in");
		return TW_INVALID_PARAM;
	}
	ds = twDataShape_Create(twDataShapeEntry_Create("tid", NULL, TW_STRING));
	if (ds) {
		twInfoTable *  it = NULL;
		twDataShape_AddEntry(ds, twDataShapeEntry_Create("command", NULL, TW_STRING));
		it = twInfoTable_Create(ds);
		if (it) {
			twInfoTableRow * row = twInfoTableRow_Create(twPrimitive_CreateFromString(tid, TRUE));
			if (row) {
				twInfoTable * result = NULL;
				twInfoTableRow_AddEntry(row, twPrimitive_CreateFromString(msg, TRUE));
				twInfoTable_AddRow(it,row);
				twApi_InvokeService(TW_THING, thingName, "TunnelCommandFromEdge", it, &result, -1, FALSE);
				twInfoTable_Delete(it);
				if (result) twInfoTable_Delete(result);
			}
		}
	}
	return TW_OK;
}

/***************************************/
/*      Tunnel Service Callbacks       */
/***************************************/
enum msgCodeEnum CompleteTunnel(const char * entityName, twInfoTable * params, twInfoTable ** content) {
	char * tid = NULL;
	char * peerName = NULL;
	char * connection = NULL;
	cJSON * connJson = NULL;
	cJSON * tmp = NULL;
	char * msg = NULL;
	twTunnel * t = NULL;
	TW_LOG(TW_DEBUG,"CompleteTunnel: Service callback invoked with entityName: %s", entityName ? entityName : "NULL");
	/* Inputs */ 
	twInfoTable_GetString(params, "tid", 0, &tid);
	twInfoTable_GetString(params, "peer_name", 0, &peerName);
	twInfoTable_GetString(params, "connection",0,&connection);
	connJson = cJSON_Parse(connection);
	if (connJson) tmp = cJSON_GetObjectItem(connJson, "reason");
	if (tmp && tmp->valuestring) msg = duplicateString(tmp->valuestring);
	cJSON_Delete(connJson);
	TW_FREE(connection);
	/* Outputs */
	*content = NULL;
	/* Perform the function */
	if (!tid || !peerName) {
		TW_LOG(TW_ERROR,"CompleteTunnel: Missing tid");
		if (tid) TW_FREE(tid);
		if (peerName) TW_FREE(peerName);
		return TWX_BAD_REQUEST;
	}
	twMutex_Lock(tm->mtx);
	t = getTunnelFromId(tid, NULL);
	if (!t) {
		TW_LOG(TW_ERROR,"CompleteTunnel: Error getting tunnel from tid: %s", tid);
		TW_FREE(tid);
		TW_FREE(peerName);
		twMutex_Unlock(tm->mtx);
		return TWX_BAD_REQUEST;
	}
	if (peerName && t->peerName) TW_FREE(t->peerName);
	t->peerName = peerName;
	if (msg && t->msg) {
		TW_FREE(t->msg);
		t->msg = NULL;
	}
	t->msg = msg;
	TW_LOG(TW_AUDIT, "TUNNEL ENDED. Entity: %s, Peer: %s, tid: %s, target: %s:%d", entityName, peerName, t->tid, t->host, t->port);
	t->markForClose = TRUE;
	TW_FREE(tid);
	twMutex_Unlock(tm->mtx);
	return TWX_SUCCESS;
}

enum msgCodeEnum StartTunnel(const char * entityName, twInfoTable * params, twInfoTable ** content) {
	twTunnel * t = NULL;
	/* Inputs */ 

	/* Outputs */
	*content = NULL;
	/* Perform the function */
	t = twTunnel_Create(entityName, params);
	if (!t) {
		TW_LOG(TW_ERROR,"StartTunnel: Error starting tunnel");
		return TWX_INTERNAL_SERVER_ERROR;
	}
	TW_LOG(TW_AUDIT, "TUNNEL CREATED. Entity: %s, tid: %s, target: %s:%d", entityName, t->tid, t->host, t->port);
	return TWX_SUCCESS;
}

enum msgCodeEnum TunnelCommandToEdge(const char * entityName, twInfoTable * params, twInfoTable ** content) {
	char * tid = NULL;
	char * command = NULL;
	twTunnel * t = NULL;
	/* Inputs */ 
	twInfoTable_GetString(params, "tid", 0, &tid);
	twInfoTable_GetString(params, "command", 0, &command);
	/* Outputs */
	*content = NULL;
	/* Perform the function */
	if (!tid || !command) {
		TW_LOG(TW_ERROR,"TunnelCommandToEdge: Missing tid or command");
		if (tid) TW_FREE(tid);
		if (command) TW_FREE(command);
		return TWX_BAD_REQUEST;
	}
	twMutex_Lock(tm->mtx);
	t = getTunnelFromId(tid, NULL);
	twMutex_Unlock(tm->mtx);
	if (!t) {
		TW_LOG(TW_ERROR,"TunnelCommandToEdge: Error getting tunnel from tid: %s", tid);
		TW_FREE(tid);
		TW_FREE(command);
		return TWX_BAD_REQUEST;
	}
	if (!strcmp(command,"CONNECT")) {
		t->firstPeerConnect = TRUE;
		/* Execute the command */
		switch (t->typeEnum) {
		case TCP_TUNNEL:
			if (t->numConnects != 0) {
				int res = 0;
				if (t->numConnects > 0) t->numConnects--;
				res = twSocket_Reconnect((twSocket *)t->localSocket);
				if (res) {
					TW_LOG(TW_ERROR,"TunnelCommandToEdge: Error connecting to %s:%d. tid: %s.  Error: %d", t->host, t->port, tid, res);
					TW_FREE(tid);
					TW_FREE(command);
					return TWX_INTERNAL_SERVER_ERROR;
				}
				t->disconnectedByPeer = FALSE;
				TW_LOG(TW_DEBUG,"TunnelCommandToEdge: Connected to %s:%d. tid: %s.", t->host, t->port, tid);
			} else {
				TW_LOG(TW_WARN,"TunnelCommandToEdge: Exceeded allowable number of connect attempts for tid: %s", tid);
				TW_FREE(tid);
				TW_FREE(command);
				if (t->msg) {
					TW_FREE(t->msg);
					t->msg = NULL;
				}
				t->msg = duplicateString("Exceeded allowable number of connect attempts");
				t->markForClose = TRUE;
				return TWX_BAD_REQUEST;
			}
			break;
		case FILE_TUNNEL:
			t->numConnects = 1;
			t->disconnectedByPeer = FALSE;
			break;
		case UDP_TUNNEL:
		case SERIAL_TUNNEL:
		default:
			/* TODO: support udp & serial */
			twTunnel_Delete(t);
			TW_LOG(TW_ERROR, "TunnelCommandToEdge: Unsupported tunnel type: %s", t->type);
			return TWX_BAD_REQUEST;
		}
	} else if (!strcmp(command,"DISCONNECT")) {
		int res = 0;
		switch (t->typeEnum) {
		case TCP_TUNNEL:
			res = twSocket_Close((twSocket *)t->localSocket);
			t->disconnectedByPeer = TRUE;
			if (res) {
				TW_LOG(TW_ERROR,"TunnelCommandToEdge: Error disconnecting from %s:%d. tid: %s.  Error: %d", t->host, t->port, tid, res);
				TW_FREE(tid);
				TW_FREE(command);
				return TWX_INTERNAL_SERVER_ERROR;
			}
			TW_LOG(TW_DEBUG,"TunnelCommandToEdge: Disconnected from %s:%d. tid: %s.", t->host, t->port, tid);
			break;
		case FILE_TUNNEL:
			t->numConnects = 0;
			t->disconnectedByPeer = TRUE;
			break;
		case UDP_TUNNEL:
		case SERIAL_TUNNEL:
		default:
			/* TODO: support udp & serial */
			twTunnel_Delete(t);
			TW_LOG(TW_ERROR, "TunnelCommandToEdge: Unsupported tunnel type: %s", t->type);
			return TWX_BAD_REQUEST;
		}
	} else {
		TW_LOG(TW_ERROR,"TunnelCommandToEdge: Unknown command %s", command);
		TW_FREE(tid);
		TW_FREE(command);
		return TWX_BAD_REQUEST;
	}
	return TWX_SUCCESS;
}

/********************************/
/* Websocket Callback Functions */
/********************************/
typedef struct twTunnelCallback {
	tunnel_cb cb;
	char * id;
	void * userdata;
} twTunnelCallback;

twTunnelCallback * twTunnelCallback_Create(tunnel_cb cb, char * id, void * userdata) {
	twTunnelCallback * tmp = (twTunnelCallback *)TW_CALLOC(sizeof(twTunnelCallback), 1);
	if (!tmp) {
		TW_LOG(TW_ERROR,"twTunnelCallback_Create: Error allocating memory");
		return NULL;
	}
	tmp->cb = cb;
	tmp->id = duplicateString(id);
	tmp->userdata = userdata;
	return tmp;
}

void twTunnelCallback_Delete(void * cb) {
	twTunnelCallback * tmp = (twTunnelCallback *)cb;
	if (!cb) return;
	if (tmp->id) TW_FREE(tmp->id);
	TW_FREE(tmp);
}

int twTunnelManager_OnWsOpenCallback(struct twWs * ws) {
	return 0;
}

int twTunnelManager_OnWsCloseCallback(struct twWs * ws, const char *at, size_t length) {
	twTunnel * t = NULL;
	/* Check to see if we are getting called because we closed this ourselves */
	char msg[128];
	strncpy(msg,at, length > 127 ? 127 : length);
	if (strstr(msg, "Tunnel stopped by edge")) {
		/* This was closed manually and we should just return */
		return 0;
	}
	/* If we are here, it was closed by the server, so mark it for close now */
	t = getTunnelFromWebsocket(ws);
	if (!t) {
		TW_LOG(TW_WARN,"twTunnelManager_OnWsCloseCallback: couldn't get tunnel id from websocket");
		return 0;
	}
	if (t->msg) {
		TW_FREE(t->msg);
		t->msg = NULL;
	}
	t->msg = duplicateString("Websocket was closed");
	t->markForClose = TRUE;
	return 0;
}

int twTunnelManager_OnWsDataCallback (struct twWs * ws, const char *at, size_t length) {
	twTunnel * t = NULL;
	unsigned long len = 0;
	int res = 0;
	t = getTunnelFromWebsocket(ws);
	if (!t || !at) {
		TW_LOG(TW_ERROR,"twTunnelManager_OnWsDataCallback: NULL input params found");
		return 0;
	}
	len = t->chunksize;
	if (base64_decode((const unsigned char *)at, length, (unsigned char *)t->writeBuffer, &len)) {
		TW_LOG(TW_ERROR,"twTunnelManager_OnWsDataCallback: Error decoding data");
		return 1;
	}
	switch (t->typeEnum) {
	case TCP_TUNNEL:
		/* Have we connected yet? */
		if (((twSocket *)t->localSocket)->state == CLOSED) {
			if (t->numConnects != 0) {
				if (t->numConnects > 0) t->numConnects--;
				res = twSocket_Reconnect((twSocket *)t->localSocket);
				if (res) {
					TW_LOG(TW_ERROR,"twTunnelManager_OnWsDataCallback: Error connecting to %s:%d. tid: %s.  Error: %d", t->host, t->port, t->tid, res);
					return 1;
				}
				TW_LOG(TW_DEBUG,"twTunnelManager_OnWsDataCallback: Connected to %s:%d. tid: %s.", t->host, t->port, t->tid);
			} else {
				TW_LOG(TW_WARN,"twTunnelManager_OnWsDataCallback: Exceeded allowable number of connect attempts for tid: %s", t->tid);
				if (t->msg) {
					TW_FREE(t->msg);
					t->msg = NULL;
				}
				t->msg = duplicateString("Exceeded allowable number of connect attempts");
				t->markForClose = TRUE;
				return 1;
			}
		}
		res = twSocket_Write((twSocket *)t->localSocket, t->writeBuffer, len, t->readTimeout);
		if (res != len) {
			TW_LOG(TW_ERROR,"twTunnelManager_OnWsDataCallback: Error writing to %s:%d. tid: %s.  Error: %d", t->host, t->port, t->tid, res);
			if (res < 0) {
				t->disconnectedByPeer = FALSE;
				sendTunnelCommand(t->thingName, t->tid, "DISCONNECT");
			}
			return 0;
		}
		break;
	case FILE_TUNNEL:
		{
		TW_FILE_HANDLE f = 0;
		if (t->localSocket == NULL) {
			TW_LOG(TW_ERROR,"twTunnelManager_OnWsDataCallback: NULL file handle pointer");
			return 0;
		}
		if (t->port == 1) {
			TW_LOG(TW_ERROR,"twTunnelManager_OnWsDataCallback: tid: %s. File %s is open for read, but received data", t->tid, t->host);
			return 0;
		}
		f = *(TW_FILE_HANDLE *)t->localSocket;
		if (TW_FWRITE(t->writeBuffer, 1, len, f) != len) {
			TW_LOG(TW_WARN,"twTunnelManager_OnWsDataCallback: Error writing to file %s tid: %s", t->host, t->tid);
			if (t->msg) {
				TW_FREE(t->msg);
				t->msg = NULL;
			}
			t->msg = duplicateString("Error writing to file");
			t->markForClose = TRUE;
			return 1;
		}
		return 0;
		}
		break;
	case UDP_TUNNEL:
	case SERIAL_TUNNEL:
	default:
		/* TODO: support udp & serial */
		twTunnel_Delete(t);
		TW_LOG(TW_ERROR, "twTunnel_Create: Unsupported tunnel type: %s", t->type);
		return 0;
	}
	TW_LOG(TW_TRACE,"twTunnelManager_OnWsDataCallback: Wrote %d bytes to local socket. for tid: %s", res, t->tid);
	t->bytesRcvd += res;
	return 0;
}

/***************************************/
/*           Tunnel Functions          */
/***************************************/
twTunnel * twTunnel_Create(const char * thingName, twInfoTable * it) {
	twTunnel * t = NULL;
	char * connection = NULL;
	cJSON * connJson = NULL;
	cJSON * tmp = NULL;
	int res = 0;
	char * resource = NULL;
	if (!thingName || !it || !tm) {
		TW_LOG(TW_ERROR, "twTunnel_Create: NULL input parameter");
		return NULL;
	}
	t = (twTunnel *)TW_CALLOC(sizeof(twTunnel), 1);
	if (!t) {
		TW_LOG(TW_ERROR, "twTunnel_Create: Error allocating storage");
		return NULL;
	}
	/* set up some defaults */
	t->chunksize = 8192;
	t->idleTimeout = 30000;
	t->readTimeout = 10;
	t->numConnects = 1;
	/* Get our tunnel config params */
	t->thingName = duplicateString(thingName);
	twInfoTable_GetString(it, "tid",0,&t->tid);
	twInfoTable_GetString(it, "type",0,&t->type);
	twInfoTable_GetInteger(it, "chunksize",0,&t->chunksize);
	twInfoTable_GetInteger(it, "idle_timeout",0,&t->idleTimeout);
	twInfoTable_GetInteger(it, "read_timeout",0,&t->readTimeout);
	t->disconnectedByPeer = TRUE;

	/* Allocate our buffers */
	t->readBuffer = (char *)TW_CALLOC(t->chunksize,1);
	t->writeBuffer = (char *)TW_CALLOC(t->chunksize,1);
	t->readBase64 = (char *)TW_CALLOC((t->chunksize * 4)/3,1);
	/* Check some required params */
	if ( !t->tid || !t->type || !t->readBuffer || !t->writeBuffer || !t->readBase64) {
		twTunnel_Delete(t);
		TW_LOG(TW_ERROR, "twTunnel_Create: Missing required params");
		return NULL;
	}
	TW_LOG(TW_DEBUG, "twTunnel_Create: Creating tunnel. tid: ", t->tid);
	/* 
	Create the websocket at /Thingworx/WSTunnelServer/" + tid but
	don't connect yet.  Want to leave connection to the tasker so that
	we don't block the original websocket connection to the server
	*/
	resource = (char *)TW_CALLOC(strlen("/Thingworx/WSTunnelServer/") + strlen(t->tid) + 1, 1);
	if (!resource) {
		twTunnel_Delete(t);
		TW_LOG(TW_ERROR, "twTunnel_Create: Error allocating memory");
		return NULL;
	}
	strcpy(resource,"/Thingworx/WSTunnelServer/");
	strcat(resource, t->tid);
	res = twWs_Create(tm->info->ws_host, tm->info->ws_port, resource, tm->info->appkey, NULL, (t->chunksize * 4)/3, (t->chunksize * 4)/3, &t->ws);
	TW_FREE(resource);
	if (res) {
		twTunnel_Delete(t);
		TW_LOG(TW_ERROR, "twTunnel_Create: Error creating websocket");
		return NULL;
	}
	/* Set up any additonal connection info */
	if (tm->info) {
#ifdef ENABLE_HTTP_PROXY_SUPPORT
		if (tm->info->proxy_host && tm->info->proxy_port) twSocket_SetProxyInfo(t->ws->connection->connection, tm->info->proxy_host, tm->info->proxy_port,
																				tm->info->proxy_pwd, tm->info->proxy_user);
#endif
		if (tm->info->ca_cert_file) twTlsClient_UseCertificateChainFile(t->ws->connection, tm->info->ca_cert_file, 0);
		if (tm->info->client_cert_file) twTlsClient_UseCertificateFile(t->ws->connection, tm->info->client_cert_file, 0);
		if (tm->info->client_key_file && tm->info->client_key_passphrase) {
			twTlsClient_SetDefaultPasswdCbUserdata(t->ws->connection, tm->info->client_key_passphrase);
			twTlsClient_UsePrivateKeyFile(t->ws->connection, tm->info->client_key_file, 0);
		}
		if (tm->info->disableEncryption) twTlsClient_DisableEncryption(t->ws->connection);
		if (tm->info->selfsignedOk) twTlsClient_SetSelfSignedOk(t->ws->connection);
		if (tm->info->doNotValidateCert) twTlsClient_DisableCertValidation(t->ws->connection);
		if (tm->info->fips_mode) twTlsClient_EnableFipsMode(t->ws->connection);
		twTlsClient_SetX509Fields(t->ws->connection, tm->info->subject_cn, tm->info->subject_o, tm->info->subject_ou, 
														tm->info->issuer_cn, tm->info->issuer_o, tm->info->issuer_ou);
	}

	/* Set the type enum */
	lowercase(t->type);
	if (!strcmp(t->type, "file")) t->typeEnum = FILE_TUNNEL;
	else if (!strcmp(t->type, "udp")) t->typeEnum = UDP_TUNNEL;
	else if (!strcmp(t->type, "serial")) t->typeEnum = SERIAL_TUNNEL;
	else t->typeEnum = TCP_TUNNEL;

	/* Create the local connection but don't connect yet */
	twInfoTable_GetString(it, "connection",0,&connection);
	if (!connection) {
		twTunnel_Delete(t);
		TW_LOG(TW_ERROR, "twTunnel_Create: No connection info found");
		return NULL;
	}
	/* Convert to cJSON struct */
	connJson = cJSON_Parse(connection);
	if (!connJson) {
		twTunnel_Delete(t);
		TW_LOG(TW_ERROR, "twTunnel_Create: Error converting %s to JSON struct", connection);
		TW_FREE(connection);
		return NULL;
	}
	tmp = cJSON_GetObjectItem(connJson, "message");
	if (tmp && tmp->valuestring) t->msg = duplicateString(tmp->valuestring);
	switch (t->typeEnum) {
	case TCP_TUNNEL:
		tmp = cJSON_GetObjectItem(connJson, "host");
		if (tmp && tmp->valuestring) t->host = duplicateString(tmp->valuestring);
		tmp = cJSON_GetObjectItem(connJson, "port");
		if (tmp && tmp->valueint) t->port = tmp->valueint;
		tmp = cJSON_GetObjectItem(connJson, "num_connects");
		if (tmp && tmp->valueint) t->numConnects = tmp->valueint;
		cJSON_Delete(connJson);
		if (!t->host || !t->port) {
			twTunnel_Delete(t);
			TW_LOG(TW_ERROR, "twTunnel_Create: Missing host or port  params");
			TW_FREE(connection);
			return NULL;
		}
		t->localSocket = twSocket_Create(t->host, t->port, 0);
		if (!t->localSocket) {
			twTunnel_Delete(t);
			TW_LOG(TW_ERROR, "twTunnel_Create: Error creating socket for %s:%d", t->host, t->port);
			TW_FREE(connection);
			return NULL;
		}
		break;
	case FILE_TUNNEL:
		{
		TW_FILE_HANDLE * f = (TW_FILE_HANDLE *)TW_CALLOC(sizeof(TW_FILE_HANDLE), 1);
		char fileExists = FALSE;
		char owrite = FALSE;
		double offset = 0.0;
		/* Use host and port for filename and mode */
		char * mode = NULL;
		char * overwrite = NULL;
		tmp = cJSON_GetObjectItem(connJson, "offset");
		if (tmp && tmp->valuedouble) offset = tmp->valuedouble;
		tmp = cJSON_GetObjectItem(connJson, "mode");
		if (tmp && tmp->valuestring) mode = duplicateString(tmp->valuestring);	
		tmp = cJSON_GetObjectItem(connJson, "overwrite");
		if (tmp && tmp->valuestring) overwrite = duplicateString(tmp->valuestring);			
		if (!overwrite) overwrite = duplicateString("true");
		tmp = cJSON_GetObjectItem(connJson, "filename");
		if (tmp && tmp->valuestring) t->host = duplicateString(tmp->valuestring);	
		cJSON_Delete(connJson);
		if (!t->host || !mode || !f) {
			twTunnel_Delete(t);
			TW_LOG(TW_ERROR, "twTunnel_Create: Missing filename or mode  params");
			TW_FREE(connection);
			return NULL;
		}
		if (!strcmp(mode,"read")) t->port = 1;
		if (!strcmp(overwrite, "true")) owrite = TRUE;
		TW_FREE(mode);
		TW_FREE(overwrite);
		/* 
		See if the file exists. If this is a read and it opened we are good, if it exists
		and we are writing, we need to have overwrite enabled
		*/
		fileExists = twDirectory_FileExists(t->host);
		if (!fileExists && t->port == 1) {
			TW_LOG(TW_ERROR, "twTunnel_Create: File %s doesn't exist for reading", t->host);
			twTunnel_Delete(t);
			TW_FREE(connection);
			return NULL;
		}
		if (fileExists && t->port == 0 && !owrite) {
			TW_LOG(TW_ERROR, "twTunnel_Create: File %s exists and overwrite is not enabled", t->host);
			twTunnel_Delete(t);
			TW_FREE(connection);
			return NULL;
		}
		/* Open the file */
		*f = TW_FOPEN(t->host, t->port ? "rb" : "a+b");
		if (f <= 0) {
			TW_LOG(TW_ERROR, "twTunnel_Create: Error opening file: %s for %s", t->host, t->port ? "reading" : "writing");
			twTunnel_Delete(t);
			TW_FREE(connection);
			return NULL;
		}
		res = TW_FSEEK(*f, (uint64_t)offset, SEEK_SET);
		if (res) {
			TW_LOG(TW_ERROR, "twTunnel_Create: Error seeking in file %s to %llu.  Error: %d", t->host, offset, twDirectory_GetLastError());
			twTunnel_Delete(t);
			TW_FREE(connection);
			return NULL;
		}
		t->localSocket = f;
		TW_LOG(TW_DEBUG, "twTunnel_Create: Opened file: %s for %s", t->host, t->port ? "reading" : "writing");
		}
		break;
	case UDP_TUNNEL:
	case SERIAL_TUNNEL:
	default:
		/* TODO: support udp & serial */
		twTunnel_Delete(t);
		TW_LOG(TW_ERROR, "twTunnel_Create: Unsupported tunnel type: %s", t->type);
		TW_FREE(connection);
		cJSON_Delete(connJson);
		return NULL;
	}
	TW_FREE(connection);

	/* Register the callback functions with the websocket */
	twWs_RegisterConnectCallback(t->ws, twTunnelManager_OnWsOpenCallback);
	twWs_RegisterCloseCallback(t->ws, twTunnelManager_OnWsCloseCallback);
	twWs_RegisterTextMessageCallback(t->ws, twTunnelManager_OnWsDataCallback);
	/* Add this tunnel to the tunnel manager's list and start it */
	if (twTunnelManager_AddTunnel(t)) {
		return NULL;
	}
	TW_LOG(TW_DEBUG,"twTunnel_Create: Succesfully created tunnel with tid: %s", t->tid);
	return t;
}

void twTunnel_Delete(void * tunnel) {
	twTunnel * t = (twTunnel *)tunnel;
	if (!t) return;
	TW_LOG(TW_TRACE,"twTunnel_Delete: Deleting tunnel. tid: %s", t->tid ? t->tid : "UNKNOWN");
	if (t->ws) {
		if (twWs_IsConnected(t->ws)) twWs_Disconnect(t->ws, NORMAL_CLOSE, "Tunnel stopped by edge");
		twWs_Delete(t->ws);
		t->ws = NULL;
	}
	if (t->localSocket) {
		switch (t->typeEnum) {
			case TCP_TUNNEL:
				twSocket_Delete((twSocket *)t->localSocket);
				t->localSocket = NULL;
				break;
			case FILE_TUNNEL:
				{
				TW_FILE_HANDLE f = *(TW_FILE_HANDLE *)t->localSocket;
				TW_FCLOSE(f);
				TW_FREE(t->localSocket);
				t->localSocket = NULL;
				}
				break;
			case UDP_TUNNEL:
			case SERIAL_TUNNEL:
			default:
				/* TODO: support udp & serial */
				break;
		}
	}
	if (t->thingName) TW_FREE(t->thingName);
	if (t->host) TW_FREE(t->host);
	if (t->tid) TW_FREE(t->tid);
	if (t->type) TW_FREE(t->type);
	if (t->readBuffer) TW_FREE(t->readBuffer);
	if (t->readBase64) TW_FREE(t->readBase64);
	if (t->writeBuffer) TW_FREE(t->writeBuffer);
	if (t->peerName) TW_FREE(t->peerName);
	if (t->msg) TW_FREE(t->msg);
	TW_FREE(t);
}

int twTunnelManager_DeleteInstance(twTunnelManager * mgr) {
	if (!mgr && tm) mgr = tm;
	if (mgr) {
		if (mgr == tm) twTunnelManager_StopAllTunnels();
		if (mgr->mtx) twMutex_Lock(mgr->mtx);
		if (mgr->openTunnels) twList_Delete(mgr->openTunnels);
		if (mgr->callbacks) twList_Delete(mgr->callbacks);
		/* Only delete the connection info if we have created our own copy */
		if (mgr->info && mgr->info != twApi_GetConnectionInfo()) twConnectionInfo_Delete(mgr->info);
		if (mgr->mtx) twMutex_Unlock(mgr->mtx);
		if (mgr == tm) tm = NULL;
		twMutex_Delete(mgr->mtx);
		TW_FREE(mgr);
		mgr = NULL;
	}
	return TW_OK;
}

/********************************/
/*        API Functions         */
/********************************/
int twTunnelManager_Create() {
	twTunnelManager * tmp = NULL;
	if (tm) {
		TW_LOG(TW_DEBUG,"twTunnelManager_Create: Tunnel Manager singleton already exists");
		return TW_OK;
	}
	tmp = (twTunnelManager *)TW_CALLOC(sizeof(twTunnelManager), 1);
	if (!tmp) {
		TW_LOG(TW_ERROR,"twTunnelManager_Create: Unable to allocate memory");
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	tmp->mtx = twMutex_Create();
	/* Get the current API connection settings */
	tmp->info = twApi_GetConnectionInfo();
	tmp->openTunnels = twList_Create(twTunnel_Delete);
	tmp->callbacks = twList_Create(twTunnelCallback_Delete);
	if (!tmp->mtx || !tmp->openTunnels || !tmp->info || !tmp->callbacks ||
		!tmp->info->appkey || !tmp->info->ws_host || !tmp->info->ws_port || !tmp->info) {
		/* Clean up and get out */
		twTunnelManager_DeleteInstance(tmp);
		return TW_ERROR_ALLOCATING_MEMORY;
	}
#ifdef ENABLE_TASKER
	/* Initalize our tasker functon */
	twApi_CreateTask(5, &twTunnelManager_TaskerFunction);
#endif
	/* Set the singleton */
	tm = tmp;
	return TW_OK;
}

int twTunnelManager_Delete() {
	return twTunnelManager_DeleteInstance(tm);
}

void twTunnelManager_TaskerFunction(DATETIME now, void * params) {
	/* 
	Iterate through our tunnel list and force a read on the websocket
	and read from the local socket. If there is data on the local socket,
	write it out to the websocket.  Need to be careful on the websocket since
	the read may cause a callback and we already have the mutex, so we must
	avoid deadlocks.  Also want to mark idle tunnels for deletion.
	*/
	ListEntry * le = NULL;
	twTunnel * t = NULL;
	int res = 0;
	if (!tm) return;
	twMutex_Lock(tm->mtx);
	le = twList_Next(tm->openTunnels, NULL);
	while (le && le->value) {
		t = (twTunnel *) le->value;
		/* Have we been marked for close? */
		if (t->markForClose) {
			char * id = duplicateString(t->tid);
			/* Advance our list since Stop Tunnel will delete this entry */
			le = twList_Next(tm->openTunnels, le);
			TW_LOG(TW_DEBUG,"twTunnelManager_TaskerFunction: Tunnel %d ws marked for close", t->tid);
			/* Send a message to the server */
			sendTunnelCommand(t->thingName, id, "CANCEL");
			/* Delete this tunnel */
			twMutex_Unlock(tm->mtx);
			twTunnelManager_StopTunnel(id, NULL);
			twMutex_Lock(tm->mtx);
			TW_FREE(id);
			continue;
		}
		/* Have we connected the websocket yet? */
		if (!t->isActive) {
			/* Start the websocket */
			int res = twWs_Connect(t->ws, twcfg.connect_timeout);
			if (res) {
				TW_LOG(TW_ERROR,"twTunnelManager_TaskerFunction: Error opening websocket. tid: %s.  Error: %d", t->tid, res);
				le = twList_Next(tm->openTunnels, le);
				/* Send a message to the server */
				sendTunnelCommand(t->thingName, t->tid, "CANCEL");
				/* Delete this tunnel */
				if (t->msg) {
					TW_FREE(t->msg);
					t->msg = NULL;
				}
				t->msg = duplicateString("Websocket connect failed");
				t->markForClose = TRUE;
				continue;
			}
			TW_LOG(TW_AUDIT, "TUNNEL STARTED. Entity: %s, tid: %s, target: %s:%d", t->thingName, t->tid, t->host, t->port);
			t->isActive = TRUE;
		}
		switch (t->typeEnum) {
			case TCP_TUNNEL:
				/* Have we connected locally yet? */
				if (((twSocket *)t->localSocket)->state == CLOSED) {
					if (t->disconnectedByPeer) {
						if (t->numConnects != 0) {
							if (t->numConnects > 0) t->numConnects--;
							res = twSocket_Reconnect((twSocket *)t->localSocket);
							if (res) {
								TW_LOG(TW_ERROR,"twTunnelManager_TaskerFunction: Error connecting to %s:%d. tid: %s.  Error: %d", t->host, t->port, t->tid, res);
							} else TW_LOG(TW_DEBUG,"twTunnelManager_TaskerFunction: Connected to %s:%d. tid: %s.", t->host, t->port, t->tid);
							t->disconnectedByPeer = FALSE;
						} else {
							TW_LOG(TW_WARN,"twTunnelManager_TaskerFunction: Exceeded allowable number of connect attempts for tid: %s", t->tid);
							le = twList_Next(tm->openTunnels, le);
							sendTunnelCommand(t->thingName, t->tid, "CANCEL");
							/* Delete this tunnel */
							if (t->msg) {
								TW_FREE(t->msg);
								t->msg = NULL;
							}
							t->msg = duplicateString("Exceeded allowable number of connect attempts");
							t->markForClose = TRUE;
							continue;
						}
					} else {
						TW_LOG(TW_DEBUG,"twTunnelManager_TaskerFunction: Local socket disconnected for tid: %s", t->tid);
						le = twList_Next(tm->openTunnels, le);
						sendTunnelCommand(t->thingName, t->tid, "DISCONNECT");
						continue;
					}
				}
				/* Check for local data available */
				if (((twSocket *)t->localSocket)->state == OPEN && twSocket_WaitFor((twSocket *)t->localSocket, 0)) {
					/* We have data on the local socket but need to account for base 64 expansion */
					int bytesRead = twSocket_Read((twSocket *)t->localSocket, t->readBuffer, t->chunksize - 16, t->readTimeout);
					if (bytesRead > 0){
						/* Need to base64 encode this and send as text */
						unsigned long len = (t->chunksize * 4)/3;
						if (!base64_encode((const unsigned char *)t->readBuffer, bytesRead, t->readBase64, &len)) {
							twWs_SendMessage(t->ws, t->readBase64, len, TRUE);
							t->bytesSent += bytesRead;
						}
						TW_LOG(TW_TRACE,"twTunnelManager_TaskerFunction: Sent %d bytes over websocket. for tid: %s", bytesRead, t->tid);
						t->idleTime = twGetSystemMillisecondCount() + t->idleTimeout;
					} else {

					}
				}
				break;
			case FILE_TUNNEL:
				{
				TW_FILE_HANDLE f = 0;
				int bytesRead = 0;
				if (t->localSocket == NULL) {
					TW_LOG(TW_ERROR,"twTunnelManager_TaskerFunction: NULL file handle pointer");
					break;;
				}
				if (t->port == 0 || t->disconnectedByPeer) {
					/* This is a write of we are being throttled by server so nothing to do here */
					break; ;
				}
				f = *(TW_FILE_HANDLE *)t->localSocket;
				bytesRead = (TW_FREAD(t->readBuffer, 1, t->chunksize - 16, f));
				if (bytesRead > 0){
					/* Need to base64 encode this and send as text */
					unsigned long len = (t->chunksize * 4)/3;
					if (!base64_encode((const unsigned char *)t->readBuffer, bytesRead, (unsigned char *)t->readBase64, &len)) {
						twWs_SendMessage(t->ws, t->readBase64, len, TRUE);
						t->bytesSent += bytesRead;
					}
					TW_LOG(TW_TRACE,"twTunnelManager_TaskerFunction: Sent %d bytes over websocket. for tid: %s", bytesRead, t->tid);
					t->idleTime = twGetSystemMillisecondCount() + t->idleTimeout;
				} else {
					le = twList_Next(tm->openTunnels, le);
					TW_LOG(TW_WARN,"twTunnelManager_TaskerFunction: Error reading from file.  tid: %s.  Shutting tunnel down", t->tid);
					t->markForClose = TRUE;
					continue;
				}
				}
				break;
			case UDP_TUNNEL:
			case SERIAL_TUNNEL:
			default:
				/* TODO: support udp & serial */
				break;
		}
		/* Check for idle tunnels */
		if (twGetSystemMillisecondCount() > t->idleTime) {
			le = twList_Next(tm->openTunnels, le);
			TW_LOG(TW_WARN,"twTunnelManager_TaskerFunction: Found idle tunnel.  tid: %s.  Shutting tunnel down", t->tid);
			sendTunnelCommand(t->thingName, t->tid, "CANCEL");
			continue;
		}
		/* Force the read from the websocket */
		twWs_Receive(t->ws, twcfg.socket_read_timeout);
		le = twList_Next(tm->openTunnels, le);
	}
	twMutex_Unlock(tm->mtx);
}

int twTunnelManager_AddTunnel(twTunnel * tunnel) {
	ListEntry * le = NULL;
	if (!tunnel || !tunnel->ws || !tunnel->localSocket || !tm) {
		TW_LOG(TW_ERROR,"twTunnelManager_StartTunnel: Invalid parameter");
		return TW_INVALID_PARAM;
	}
	/* Set the start time to now */
	tunnel->startTime = twGetSystemTime(TRUE);
	tunnel->idleTime = twAddMilliseconds(tunnel->startTime, tunnel->idleTimeout);
	/* Add this to our list */
	twMutex_Lock(tm->mtx);
	twList_Add(tm->openTunnels, tunnel);
	/* Make any callbacks */
	le = twList_Next(tm->callbacks, NULL);
	while (le && le->value) {
		twTunnelCallback * cb = (twTunnelCallback *)le->value;
		if (cb->id && (!strcmp(tunnel->tid, cb->id) || !strcmp(cb->id,"*"))) {
			cb->cb(TRUE, tunnel->tid, tunnel->thingName, tunnel->peerName, tunnel->host, tunnel->port, 
				tunnel->startTime, tunnel->endTime, tunnel->bytesSent, tunnel->bytesRcvd, tunnel->type, tunnel->msg, cb->userdata);
		}
		le = twList_Next(tm->callbacks, le);
	}
	twMutex_Unlock(tm->mtx);
	return TW_OK;
}

int twTunnelManager_StopTunnel(char * id, char * msg) {
	ListEntry * entry = NULL;
	ListEntry * le = NULL;
	twTunnel * tmp = NULL;
	twMutex_Lock(tm->mtx);
	tmp = getTunnelFromId(id, &entry);
	if (!tmp) {
		TW_LOG(TW_WARN,"twTunnelManager_StopTunnel: Error finding tunnel with id: %s", id ? id : "NULL");
		twMutex_Unlock(tm->mtx);
		return TW_INVALID_PARAM;
	}
	/* Set the stop time */
	tmp->endTime = twGetSystemTime(TRUE);
	/* Set the msg */
	if (msg && tmp->msg) TW_FREE(tmp->msg);
	if (msg) tmp->msg = duplicateString(msg);

	/* Make any callbacks that were registered */
	le = twList_Next(tm->callbacks, NULL);
	while (le && le->value) {
		twTunnelCallback * cb = (twTunnelCallback *)le->value;
		if (cb->id && (!strcmp(id, cb->id) || !strcmp(cb->id,"*"))) {
			cb->cb(FALSE, tmp->tid, tmp->thingName, tmp->peerName, tmp->host, tmp->port, 
				tmp->startTime, tmp->endTime, tmp->bytesSent, tmp->bytesRcvd, tmp->type, tmp->msg, cb->userdata);
		}
		le = twList_Next(tm->callbacks, le);
	}
	/* Close the websocket - need to do this now to prevent a deadlock in the onClose callback */
	if (tmp->ws) twWs_Disconnect(tmp->ws, NORMAL_CLOSE, "Tunnel stopped by edge");
	/* Delete the tunnel (which closes the websocket as well) */
	twList_Remove(tm->openTunnels, entry, TRUE);
	twMutex_Unlock(tm->mtx);
	return TW_OK;
}

int twTunnelManager_StopAllTunnels() {
    TW_LOG(TW_DEBUG,"twTunnelManager_StopAllTunnels: Attempting to stop all tunnels");
	if (!tm) {
		TW_LOG(TW_ERROR,"twTunnelManager_StopAllTunnels: Tunnel Manager not initialized");
		return TW_TUNNEL_MANAGER_NOT_INITIALIZED;
	}
	twMutex_Lock(tm->mtx);
	/* Clear the tunnel list (which  deletes all tunnels as well) */
	twList_Clear(tm->openTunnels);
	twMutex_Unlock(tm->mtx);
	return TW_OK;
}

int twTunnelManager_UpdateTunnelServerInfo(char * host, uint16_t port, char * appkey) {
	if (!appkey || !host || !port) {
		return TW_INVALID_PARAM;
	}
	if (!tm || !tm->info) return TW_TUNNEL_MANAGER_NOT_INITIALIZED;

	if (tm->info->appkey) {
		TW_FREE(tm->info->appkey);
		tm->info->appkey = NULL;
	}
	if (tm->info->ws_host) {
		TW_FREE(tm->info->ws_host);
		tm->info->ws_host = NULL;
	}
	tm->info->appkey = duplicateString(appkey);
	tm->info->ws_host = duplicateString(host);
	tm->info->ws_port = port;

	if (!tm->info->appkey || !tm->info->ws_host || !tm->info->ws_port) {
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	return TW_OK;
}

int twTunnelManager_RegisterTunnelCallback(tunnel_cb cb, char * id, void * userdata) {
	twTunnelCallback * tmp = NULL;
	if (!tm) {
		TW_LOG(TW_ERROR,"twTunnelManager_RegisterTunnelCallback: Tunnel Manager not initialized");
		return TW_TUNNEL_MANAGER_NOT_INITIALIZED;
	}
	if (!cb) {
		TW_LOG(TW_ERROR,"twTunnelManager_RegisterTunnelCallback: NULL parameters found");
		return TW_INVALID_PARAM;
	}
	tmp = twTunnelCallback_Create(cb, id ? id : "*", userdata);
	if (!tmp) {
		TW_LOG(TW_ERROR,"twTunnelManager_RegisterTunnelCallback: Error allocating memory");
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	twMutex_Lock(tm->mtx);
	twList_Add(tm->callbacks, tmp);
	twMutex_Unlock(tm->mtx);
	return TW_OK;
}

int twTunnelManager_UnregisterTunnelCallback(tunnel_cb cb, char * id, void * userdata) {
	ListEntry * le = NULL;
	twTunnelCallback * tcb = NULL;
	if (!cb || !id || !tm || !tm->mtx || !tm->callbacks) {
		TW_LOG(TW_ERROR,"twTunnelManager_UnregisterTunnelCallback: Invalid params or state");
		return TW_INVALID_PARAM;
	}
	twMutex_Lock(tm->mtx);
	le = twList_Next(tm->callbacks, NULL);
	while(le && le->value) {
		tcb = (twTunnelCallback *)le->value;
		if (tcb->cb == cb && tcb->userdata == userdata && tcb->id && strcmp(tcb->id, id)==0) {
			twList_Remove(tm->callbacks, le, TRUE);
			break;
		}
		le = twList_Next(tm->callbacks, le);
	}
	twMutex_Unlock(tm->mtx);
	return TW_OK;
}


char * tunnelServices[] = {
	"StartTunnel", "CompleteTunnel", "TunnelCommandToEdge",	"SENTINEL"
};

enum msgCodeEnum tunnelServiceCallback(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content) {
	if (!entityName || !serviceName || !params) {
		TW_LOG(TW_ERROR, "tunnelServiceCallback: missing entityName, serviceName, or input params");
		return TWX_BAD_REQUEST;
	}
	if (!content) {
		TW_LOG(TW_ERROR, "tunnelServiceCallback: missing content param");
		return TWX_INTERNAL_SERVER_ERROR;
	}
	if (!strcmp(serviceName, "StartTunnel")) {
		return StartTunnel(entityName, params, content);
	} else 	if (!strcmp(serviceName, "CompleteTunnel")) {
		return CompleteTunnel(entityName, params, content);
	} else 	if (!strcmp(serviceName, "TunnelCommandToEdge")) {
		return TunnelCommandToEdge(entityName, params, content);
	} else {
		TW_LOG(TW_ERROR, "tunnelServiceCallback: Bad serviceName: %s", serviceName);
		return TWX_BAD_REQUEST;
	}
}

twActiveTunnel * twActiveTunnel_Create(twTunnel * t) {
	twActiveTunnel * a = NULL;
	if (!t) return NULL;
	a = (twActiveTunnel *)TW_CALLOC(sizeof (twActiveTunnel), 1);
	if (!a) return NULL;
	if (t->thingName) a->thingName = duplicateString(t->thingName);
	if (t->tid) a->tid = duplicateString(t->tid);
	if (t->peerName) a->peerName = duplicateString(t->peerName);
	if (t->host) a->host = duplicateString(t->host);
	if (t->type) a->type = duplicateString(t->type);
	a->port = t->port;
	a->startTime = t->startTime;
	a->endTime = t->endTime;
	return a;
}

void twActiveTunnel_Delete(void * a) {
	twActiveTunnel * t = (twActiveTunnel *)a;
	if (!t) return;
	if (t->thingName) TW_FREE(t->thingName);
	if (t->tid) TW_FREE(t->tid);
	if (t->peerName) TW_FREE(t->peerName);
	if (t->host) TW_FREE(t->host);
	if (t->type) TW_FREE(t->type);
	TW_FREE(t);
}

twActiveTunnelList * twTunnelManager_ListActiveTunnels() {
	twActiveTunnelList * a = NULL;
	ListEntry * le = NULL;
	if (!tm || !tm->openTunnels) return NULL;
	a = twList_Create(twActiveTunnel_Delete);
	if (!a) return NULL;
	twMutex_Lock(tm->mtx);
	le = twList_Next(tm->openTunnels, NULL);
	while (le && le->value) {
		twList_Add(a, twActiveTunnel_Create((twTunnel *)le->value));
		le = twList_Next(tm->openTunnels, le);
	}
	twMutex_Unlock(tm->mtx);
	return a;
}

int twTunnelManager_CheckConnectionInfo() {
	if (!tm) {
		TW_LOG(TW_ERROR,"twTunnelManager_CheckConnectionInfo: TunnelManager is not initialized");
		return TW_TUNNEL_MANAGER_NOT_INITIALIZED;
	}
	/* If we are changing the settings we don't want to change the API settings */
	if (tm->info == twApi_GetConnectionInfo()) tm->info = twConnectionInfo_Create(twApi_GetConnectionInfo());
	if (!tm->info) {
		TW_LOG(TW_ERROR,"twTunnelManager_CheckConnectionInfo: Error creating tunnel manager connection info struct");
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	return 0;
}

void twTunnelManager_SetProxyInfo(char * proxyHost, uint16_t proxyPort, char * proxyUser, char * proxyPass) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	if (tm->info->proxy_host) {
		TW_FREE(tm->info->proxy_host);
		tm->info->proxy_host = NULL;
	}
	tm->info->proxy_host = duplicateString(proxyHost);
	tm->info->proxy_port = proxyPort;
	if (tm->info->proxy_user) {
		TW_FREE(tm->info->proxy_user);
		tm->info->proxy_user = NULL;
	}
	tm->info->proxy_user = duplicateString(proxyUser);
	if (tm->info->proxy_pwd) {
		TW_FREE(tm->info->proxy_pwd);
		tm->info->proxy_pwd = NULL;
	}
	tm->info->proxy_pwd = duplicateString(proxyPass);
}

void twTunnelManager_SetSelfSignedOk(char state) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	tm->info->selfsignedOk = state;
}

void twTunnelManager_EnableFipsMode(char state) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	tm->info->fips_mode = state;
}

void twTunnelManager_DisableCertValidation(char state) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	tm->info->doNotValidateCert = state;
}

void twTunnelManager_DisableEncryption(char state) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	tm->info->disableEncryption = state;
}

void twTunnelManager_SetX509Fields(char * subject_cn, char * subject_o, char * subject_ou,
							  char * issuer_cn, char * issuer_o, char * issuer_ou) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	if (tm->info->subject_cn) {
		TW_FREE(tm->info->subject_cn);
		tm->info->subject_cn = NULL;
	}
	tm->info->subject_cn = duplicateString(subject_cn);
	if (tm->info->subject_o) {
		TW_FREE(tm->info->subject_o);
		tm->info->subject_o = NULL;
	}
	tm->info->subject_o = duplicateString(subject_o);
	if (tm->info->subject_ou) {
		TW_FREE(tm->info->subject_ou);
		tm->info->subject_ou = NULL;
	}
	tm->info->subject_ou = duplicateString(subject_ou);
	if (tm->info->issuer_cn) {
		TW_FREE(tm->info->issuer_cn);
		tm->info->issuer_cn = NULL;
	}
	tm->info->issuer_cn = duplicateString(issuer_cn);
	if (tm->info->issuer_o) {
		TW_FREE(tm->info->issuer_o);
		tm->info->issuer_o = NULL;
	}
	tm->info->issuer_o = duplicateString(issuer_o);
	if (tm->info->issuer_ou) {
		TW_FREE(tm->info->issuer_ou);
		tm->info->issuer_ou = NULL;
	}
	tm->info->issuer_ou = duplicateString(issuer_ou);
}

void twTunnelManager_LoadCACert(const char *file, int type) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	if (tm->info->ca_cert_file) {
		TW_FREE(tm->info->ca_cert_file);
		tm->info->ca_cert_file = NULL;
	}
	tm->info->ca_cert_file = duplicateString(file);
}

void twTunnelManager_LoadClientCert(char *file) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	if (tm->info->client_cert_file) {
		TW_FREE(tm->info->client_cert_file);
		tm->info->client_cert_file = NULL;
	}
	tm->info->client_cert_file = duplicateString(file);
}

void twTunnelManager_SetClientKey(const char *file, char * passphrase, int type) {
	if (twTunnelManager_CheckConnectionInfo()) return;
	if (tm->info->client_key_file) {
		TW_FREE(tm->info->client_key_file);
		tm->info->client_key_file = NULL;
	}
	tm->info->client_key_file = duplicateString(file);
	if (tm->info->client_key_passphrase) {
		TW_FREE(tm->info->client_key_passphrase);
		tm->info->client_key_passphrase = NULL;
	}
	tm->info->client_key_passphrase = duplicateString(passphrase);
}

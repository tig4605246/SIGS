/*
 *  Copyright (C) 2014 ThingWorx Inc.
 *
 *  Portable ThingWorx C SDK API layer
 */

#include "twApi.h"
#include "twOSPort.h"
#include "twLogger.h"
#include "twMessages.h"
#include "twMessaging.h"
#include "twApi.h"
#include "stringUtils.h"
#include "twList.h"
#include "twTls.h"
#include "twTasker.h"
#include "twProperties.h"
#include "twServices.h"
#include "twVersion.h"
#ifdef ENABLE_FILE_XFER
#include "twFileManager.h"
#endif
#ifdef ENABLE_TUNNELING
#include "twTunnelManager.h"
#endif
#include "twSubscribedProps.h"

/* Singleton API structure */
twApi * tw_api = NULL;

/* Global mutex used during intitialization */
TW_MUTEX twInitMutex;

/****************************************/
/**        Callback data struct        **/
/****************************************/
void deleteCallbackInfo(void * info) {
	if (info) {
		callbackInfo * tmp = (callbackInfo *)info;
		if (tmp->charateristicDefinition) {
			if (tmp->characteristicType == TW_PROPERTIES) twPropertyDef_Delete(tmp->charateristicDefinition);
			else if (tmp->characteristicType == TW_SERVICES) twServiceDef_Delete(tmp->charateristicDefinition);
		}
		TW_FREE(tmp->entityName);
		TW_FREE(tmp->charateristicName); 
		TW_FREE(tmp);
	}
}

/****************************************/
/**           Bindlist Entry           **/
/****************************************/
typedef struct bindListEntry {
	char * name;
	char needsPropertyUpdate;
} bindListEntry;

bindListEntry * bindListEntry_Create(char * entityName) {
	bindListEntry * e = NULL;
	if (!entityName) return NULL;
	e = (bindListEntry *)TW_CALLOC(sizeof(bindListEntry), 1);
	if (!e) return NULL;
	e->name = duplicateString(entityName);
	e->needsPropertyUpdate = FALSE;
	return e;
}

void bindListEntry_Delete(void * entry) {
	bindListEntry * e = (bindListEntry *)entry;
	if (!e) return;
	if (e->name) TW_FREE(e->name);
	TW_FREE (e);
}

/* Notify Property Update Handler */
enum msgCodeEnum notifyPropertyUpdateHandler(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) {
	ListEntry * le = NULL;
	if (!entityName) return TWX_BAD_REQUEST;
	twMutex_Lock(tw_api->mtx);
	le = twList_Next(tw_api->boundList, NULL);
	while (le && le->value) {
		bindListEntry * e = (bindListEntry *)le->value;
		if (strcmp(e->name, entityName) == 0) {
			e->needsPropertyUpdate = TRUE;
			break;
		}
		le = twList_Next(tw_api->boundList, le);
	}
	twMutex_Unlock(tw_api->mtx);
	return TWX_SUCCESS;	
}

void subscribedPropertyUpdateTask(DATETIME now, void * params) {
	ListEntry * le = NULL;
	le = twList_Next(tw_api->boundList, NULL);
	while (le && le->value) {
		bindListEntry * e = (bindListEntry *)le->value;
		if (e->needsPropertyUpdate) {
			/* Make the request to the server */
			twInfoTable * it = NULL;
			int res = TW_OK;
			res = twApi_InvokeService(TW_THING, e->name, "GetPropertySubscriptions", NULL, &it, -1, FALSE);
			if (res) {
				TW_LOG(TW_ERROR,"subscribedPropertyUpdateTask - Error getting subscribed properties");
			} else {
				/* Update the metadata */
				int count = 0;
				if (!it) return;
				count = twList_GetCount(it->rows);
				while (count > 0) {
					char * propName;
					char * pushType;
					double pushThreshold;
					twInfoTable_GetString(it, "edgeName", count - 1, &propName);
					twInfoTable_GetString(it, "pushType", count - 1, &pushType);
					twInfoTable_GetNumber(it, "pushThreshold", count - 1, &pushThreshold);
					/* TW_UNKNOWN_TYPE - keeps the current value, NULL description keeps the current value */
					if (twApi_UpdatePropertyMetaData(TW_THING, e->name, propName, TW_UNKNOWN_TYPE, NULL, pushType, pushThreshold)) {
						TW_LOG(TW_ERROR,"subscribedPropertyUpdateTask - Error updating metadata for property %s", propName ? propName : "UNKNOWN");
					} else {
						TW_LOG(TW_TRACE,"subscribedPropertyUpdateTask - Updated metadata for property %s", propName); 
					}
					TW_FREE(propName);
					propName = NULL;
					TW_FREE(pushType);
					pushType = NULL;
					count--;
				}
			}
			if (it) twInfoTable_Delete(it);
			e->needsPropertyUpdate = FALSE;
		}
		le = twList_Next(tw_api->boundList, le);
	}
}

/****************************************/
/**          Helper functions          **/
/****************************************/
#ifdef ENABLE_FILE_XFER
/* File Callback data structure */
extern void * fileXferCallback;
#endif

char isFileTransferService(char * service) {
	int i = 0;
	if (!service) return FALSE;
	#ifdef ENABLE_FILE_XFER
	while (strcmp(fileXferServices[i],"SENTINEL")) {
		if (!strcmp(fileXferServices[i++],service)) {
			/* This is a file transfer service */
			return TRUE;
		}
	}
	#endif
	return FALSE;
}

#ifdef ENABLE_TUNNELING
/* Tunnel callback for handling all tunneling services. */
enum msgCodeEnum tunnelServiceCallback(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content) ;
#endif

char isTunnelService(char * service) {
	int i = 0;
	if (!service) return FALSE;
	#ifdef ENABLE_TUNNELING
	while (strcmp(tunnelServices[i],"SENTINEL")) {
		if (!strcmp(tunnelServices[i++],service)) {
			/* This is a tunneling service */
			return TRUE;
		}
	}
	#endif
	return FALSE;
}

int convertMsgCodeToErrorCode(enum msgCodeEnum code) {
	int err;
	switch (code) {
		case TWX_SUCCESS:
			err = TW_OK;
			break;
		case TWX_BAD_REQUEST:
			err = TW_BAD_REQUEST;
			break;
		case TWX_UNAUTHORIZED:
			err = TW_UNAUTHORIZED;
			break;
		case TWX_BAD_OPTION:
			err = TW_ERROR_BAD_OPTION;
			break;
		case TWX_FORBIDDEN:
			err = TW_FORBIDDEN;
			break;
		case TWX_NOT_FOUND:
			err = TW_NOT_FOUND;
			break;
		case TWX_METHOD_NOT_ALLOWED:
			err = TW_METHOD_NOT_ALLOWED;
			break;
		case TWX_NOT_ACCEPTABLE:
			err = TW_NOT_ACCEPTABLE;
			break;
		case TWX_PRECONDITION_FAILED:
			err = TW_PRECONDITION_FAILED;
			break;
		case TWX_ENTITY_TOO_LARGE:
			err = TW_ENTITY_TOO_LARGE;
			break;
		case TWX_UNSUPPORTED_CONTENT_FORMAT:
			err = TW_UNSUPPORTED_CONTENT_FORMAT;
			break;
		case TWX_INTERNAL_SERVER_ERROR:
			err = TW_INTERNAL_SERVER_ERROR;
			break;
		case TWX_NOT_IMPLEMENTED:
			err = TW_NOT_IMPLEMENTED;
			break;
		case TWX_BAD_GATEWAY:
			err = TW_BAD_GATEWAY;
			break;
		case TWX_SERVICE_UNAVAILABLE:
			err = TW_SERVICE_UNAVAILABLE;
			break;
		case TWX_GATEWAY_TIMEOUT:
			err = TW_GATEWAY_TIMEOUT;
			break;
		case TWX_WROTE_TO_OFFLINE_MSG_STORE:
			err = TW_WROTE_TO_OFFLINE_MSG_STORE;
			break;
		default:
			err = TW_UNKNOWN_ERROR;
			break;
	}
	return err;
}

ListEntry * findRegisteredItem(twList * list, enum entityTypeEnum entityType, char * entityName, 
						   enum characteristicEnum charateristicType, char * characteristicName) {
	/* Get based on entity/characteristic pair */
	ListEntry * le = NULL;
	char * target = NULL;
	if (!list || !entityName || !characteristicName) {
		TW_LOG(TW_ERROR, "findRegisteredItem: NULL input parameter found");
		return 0;
	}
	le = twList_Next(list, NULL);
	while (le && le->value) {
		callbackInfo * tmp = (callbackInfo *)(le->value);
		target = tmp->charateristicName;
		if (tmp->entityType != entityType || tmp->characteristicType != charateristicType || 
			strcmp(entityName, tmp->entityName) || strcmp(characteristicName, target)) {
			le = twList_Next(list, le);
			continue;
		} else {
			return le;
		}
	}
	return NULL;
}

void * getCallbackFromList(twList * list, enum entityTypeEnum entityType, char * entityName, 
						   enum characteristicEnum charateristicType, char * characteristicName, void ** userdata) {
	/* Get based on entity/characteristic pair */
	ListEntry * le = NULL;
	void * wildcard_cb = NULL;
	void * wildcard_userdata = NULL;
	char * target = NULL;
	if (!list || !entityName || !characteristicName) {
		TW_LOG(TW_ERROR, "getCallbackFromList: NULL input parameter found");
		return 0;
	}
	le = twList_Next(list, NULL);
	while (le && le->value) {
		callbackInfo * tmp = (callbackInfo *)(le->value);
		target = tmp->charateristicName;
		if (tmp->entityType != entityType || tmp->characteristicType != charateristicType || 
			strcmp(entityName, tmp->entityName) || (strcmp(target, "*") && strcmp(characteristicName, target))) {
			le = twList_Next(list, le);
			continue;
		}
		*userdata = tmp->userdata;
		/* 
			If this is an asterisk match, keep checking to see if there is one that is an exact match 
			The wildcard can be "*", "*F", "*T", "*B" which allows the registered Thing to handle all services,
			all services except for File transfers, all services except fot Tunnels, or all services exceept for
			both Tunnel and File transfer services.
		*/
		if (target && target[0] == '*') {
			if (isTunnelService(characteristicName)) {
				if (target[1] == 'T' || target[1] == 'B') return tmp->cb;
			} else if (isFileTransferService(characteristicName)) {
				if (target[1] == 'F' || target[1] == 'B') return tmp->cb;
			} else {
				/* Save the wildcard match, but continue looking for a more exact match */
				wildcard_userdata = tmp->userdata;
				wildcard_cb = tmp->cb;
			}
			le = twList_Next(list, le);
			continue;
		} else return tmp->cb;
	}
	/* Return the wild card callback if we found one */
	if (wildcard_cb) {
		*userdata = wildcard_userdata;
		return wildcard_cb;
	}
	/* If file transfer is enabled let that system handle any unhandled file transfer services */
	#ifdef ENABLE_FILE_XFER
	if (isFileTransferService(characteristicName)) return fileXferCallback;
	#endif
	/* If tunneling is enabled let that system handle any unhandled tunnel services */
	#ifdef ENABLE_TUNNELING
	if (isTunnelService(characteristicName)) return tunnelServiceCallback;
	#endif
	*userdata = NULL;
	return NULL;
}

enum msgCodeEnum sendMessageBlocking(twMessage * msg, int32_t timeout, twInfoTable ** result) {
	DATETIME expirationTime, now;
	unsigned char onlyResponses;

	int ret = TW_OK;
	if (!tw_api || !tw_api->mh || !msg) {
		TW_LOG(TW_ERROR, "api:sendMessageBlocking: NULL api, msg or message handler pointer found");
		return TWX_UNKNOWN;
	}
	expirationTime = twGetSystemMillisecondCount();
	expirationTime = twAddMilliseconds(expirationTime, timeout);
	/* Register the response before we send to prevent a race condition */
	twMessageHandler_RegisterResponseCallback(tw_api->mh, 0, msg->requestId, expirationTime);
	/* Send the message and wait for it to be received */
	ret = twMessage_Send(msg, tw_api->mh->ws);
	if (ret == TW_OK) {
		twResponseCallbackStruct * cb = 0;
		now = twGetSystemMillisecondCount();
		while (twTimeLessThan(now, expirationTime)) {
			if (twWs_Receive(tw_api->mh->ws, twcfg.socket_read_timeout)) {
				TW_LOG(TW_WARN,"api:sendMessageBlocking: Receive failed.");
				break;
			}
			
			onlyResponses = TRUE;
			twMessageHandler_msgHandlerTask(now, &onlyResponses);
			cb = twMessageHandler_GetCompletedResponseStruct(tw_api->mh, msg->requestId);
			if (cb) break;
			now = twGetSystemMillisecondCount();
		}
		if (!cb) {
			TW_LOG(TW_WARN,"api:sendMessageBlocking: Message %d timed out", msg->requestId);
			twMessageHandler_CleanupOldMessages(tw_api->mh);
			return TWX_GATEWAY_TIMEOUT;
		} else {
			enum msgCodeEnum code = cb->code;
			TW_LOG(TW_TRACE,"api:sendMessageBlocking: Received Response to Message %d.  Code: %d", msg->requestId, code);
			if (result) *result = cb->content;
			/* If this was an auth request we need to grab the session ID */
			if (msg->type == TW_AUTH) {
				if (code == TWX_SUCCESS) { 
					TW_LOG(TW_TRACE,"api:sendMessageBlocking: AUTH Message %d succeeded", msg->requestId, code);
					tw_api->mh->ws->sessionId = cb->sessionId;
				} else {
					TW_LOG(TW_WARN,"api:sendMessageBlocking: AUTH Message %d failed.  Code:", msg->requestId, code);
				}
			}
			twMessageHandler_UnegisterResponseCallback(tw_api->mh, msg->requestId);
			return code;
		}
	} else {
		/* Message failed to send - remove it from the response callback list */
		twMessageHandler_UnegisterResponseCallback(tw_api->mh, msg->requestId);
		if (ret == TW_WROTE_TO_OFFLINE_MSG_STORE) return TWX_WROTE_TO_OFFLINE_MSG_STORE;
		return TWX_PRECONDITION_FAILED;
	}
}

enum msgCodeEnum makeRequest(enum msgCodeEnum method, enum entityTypeEnum entityType, char * entityName, 
	                         enum characteristicEnum characteristicType, char * characteristicName, 
							 twInfoTable * params, twInfoTable ** result, int32_t timeout, char forceConnect) {
	enum msgCodeEnum res = TWX_PRECONDITION_FAILED;
	twMessage * msg = NULL;
	if (!tw_api || !entityName || !characteristicName || !result) {
		TW_LOG(TW_ERROR, "api:makeRequest: NULL tw_api, entityName, charateristicName or result pointer");
		return res;
	}
	/* Check to see if we are offline and should attempt to reconnect */
	if (!twApi_isConnected()) {
		if (forceConnect) {
			if (twApi_Connect(twcfg.connect_timeout, twcfg.connect_retries)) {
				TW_LOG(TW_ERROR, "api:makeRequest: Error trying to force a reconnect");
				return TWX_SERVICE_UNAVAILABLE;
			}
		} else if (!tw_api->offlineMsgEnabled) {
			TW_LOG(TW_INFO, "api:makeRequest: Currently offline and 'forceConnect' is FALSE");
			return TWX_SERVICE_UNAVAILABLE;
		}
	}
	/* Create the Request message */
	msg = twMessage_CreateRequestMsg(method);
	if (!msg) {
		TW_LOG(TW_ERROR, "api:makeRequest: Error creating request message");
		return res;
	}
	if (timeout < 0) timeout = twcfg.default_message_timeout;
	/* Set the body of the message */
	twRequestBody_SetEntity((twRequestBody *)(msg->body), entityType, entityName);
	twRequestBody_SetCharateristic((twRequestBody *)(msg->body), characteristicType, characteristicName);
	twRequestBody_SetParams((twRequestBody *)(msg->body), twInfoTable_ZeroCopy(params));
	twMutex_Lock(tw_api->mtx);
	res = sendMessageBlocking(msg, timeout, result);
	twMutex_Unlock(tw_api->mtx);
	if (msg) twMessage_Delete(msg);
	return res;
}

enum msgCodeEnum makePropertyRequest(enum msgCodeEnum method, enum entityTypeEnum entityType, char * entityName, 
	                         char * propertyName, twPrimitive * value, twPrimitive ** result, int32_t timeout, char forceConnect) {
	twInfoTable * value_it = NULL;
	twInfoTable * result_it = NULL;
	twPrimitive * tmp = NULL;

	int index;

	enum msgCodeEnum res;
	if (!result) return TWX_PRECONDITION_FAILED;
	if (value) {
		/* An InfoTable value could indicate a few different property write scenarios. */
		if (value->type == TW_INFOTABLE) {
			/* If this is a write of several properties at once, pass along the whole InfoTable. User must pass in the */
			/* correct format. */
			if (!strcmp(propertyName,"*")) {
				value_it = twInfoTable_ZeroCopy(value->val.infotable);
			} 
			/* Special else if() to catch the special _content_ parameter in the infotable datashape entry. */
			/* This check determines if the input value is an InfoTable, and if the InfoTable has a field named _content_. */
			/* This allows the client to set a property, even if it doesn't know the properties type. */
			/* If it does have a field named _content_, then send over the whole InfoTable. */
			/* The platform knows to treat the _content_ field as a JSON param and decode */
			/* the contents correctly, based on the type of the property being PUT. */
			else if(value->val.infotable && value->val.infotable->ds && value->val.infotable->ds->numEntries == 1 && !twDataShape_GetEntryIndex(value->val.infotable->ds, "_content_", &index)) {
				value_it = twInfoTable_ZeroCopy(value->val.infotable);
			}
			/* If the value's datashape specifies a field whose name matches the propertyName, then assume that the value */
			/* parameter is an InfoTable (with a single field of type InfoTable) that wraps another InfoTable that contains */
			/* the property's new value. */
			else if(value->val.infotable && value->val.infotable->ds && value->val.infotable->ds->numEntries == 1 && !twDataShape_GetEntryIndex(value->val.infotable->ds, propertyName, &index)) {
				value_it = twInfoTable_ZeroCopy(value->val.infotable);
			}
			/* Assume that the value passed in is just the value for the property. In this case we need to wrap it in an */
			/* InfoTable with a single field specifying the property name. */
			else {
				value_it = twInfoTable_CreateFromPrimitive(propertyName, twPrimitive_ZeroCopy(value));
			}
		}
		/* Non-InfoTable properties are simply converted to an InfoTable to be serialized. */
		else {
			value_it = twInfoTable_CreateFromPrimitive(propertyName, twPrimitive_ZeroCopy(value));
		}
	}
	res = makeRequest(method, entityType, entityName, TW_PROPERTIES, propertyName, value_it, &result_it, timeout, forceConnect);
	twInfoTable_Delete(value_it);
	if (result_it) {
		/* if this is a request for all properties we need to treat the result a little differntly */
		if (!strcmp(propertyName,"")) {
			*result = twPrimitive_CreateFromInfoTable(result_it);
		} else {
			twInfoTable_GetPrimitive(result_it, propertyName, 0, &tmp);
			*result = twPrimitive_ZeroCopy(tmp);
		}
		twInfoTable_Delete(result_it);
	}
	return res;
}

int twApi_SendResponse(twMessage * msg) {
	if (tw_api && tw_api->mh && tw_api->mh->ws) return twMessage_Send(msg, tw_api->mh->ws);
	return TW_ERROR_SENDING_RESP;
}

int api_requesthandler(struct twWs * ws, struct twMessage * msg) {
	/*
	Need to look this up to see if we have a property or service callback registered.
	If not we check to see if there is a generic callback
	*/
	int res = TW_UNKNOWN_ERROR;
	twRequestBody * b = NULL;
	twMessage * resp = NULL;
	enum msgCodeEnum respCode = TWX_UNKNOWN;
	twInfoTable * result = NULL;
	void * cb = NULL;
	void * userdata = NULL;
	if (!msg || !tw_api) {
		TW_LOG(TW_ERROR,"api_requesthandler: Null msg pointer");
		return TW_INVALID_PARAM;
	}
	if (msg->type != TW_REQUEST) {
		TW_LOG(TW_ERROR,"api_requesthandler: Non Request message received");
		return TW_INVALID_MSG_TYPE;
	}
	b = (twRequestBody *) (msg->body);
	if (!b || !b->entityName) {
		TW_LOG(TW_ERROR,"api_requesthandler: No valid message body found");
		return TW_INVALID_MSG_BODY;
	}
	cb = getCallbackFromList(tw_api->callbackList, b->entityType, b->entityName, b->characteristicType, b->characteristicName, &userdata);
	if (cb) {
		switch (b->characteristicType) {
			case TW_PROPERTIES:
				{
					property_cb callback = (property_cb)cb;
					if (msg->code == TWX_PUT) {
						/*  This is a property write */
						if (!b->params) {
							TW_LOG(TW_ERROR,"api_requesthandler: Missing param in PUT message");
							return TW_INVALID_MSG_PARAMS;
						}
						respCode = callback(b->entityName, b->characteristicName, &b->params, TRUE, userdata);
						resp = twMessage_CreateResponseMsg(respCode, msg->requestId);
					} else {
						/* This is a read */
						respCode = callback(b->entityName, b->characteristicName, &result, FALSE, userdata);
						resp = twMessage_CreateResponseMsg(respCode, msg->requestId);
						if (resp && result) twResponseBody_SetContent((twResponseBody *)(resp->body), result);
					}
					break;
				}
			case TW_SERVICES:
				{
					service_cb callback = (service_cb)cb;
					respCode = callback(b->entityName, b->characteristicName, b->params, &result, userdata);
					resp = twMessage_CreateResponseMsg(respCode, msg->requestId);
					if (resp && respCode != TWX_SUCCESS && result) {
						/* try to extract a reason from the result infotable */
						char * reason = NULL;
						twInfoTable_GetString(result, "reason", 0, &reason);
						if (reason) twResponseBody_SetReason((twResponseBody *)resp->body, reason);
						TW_FREE(reason);
					}
					if (resp && result) twResponseBody_SetContent((twResponseBody *)(resp->body), result);
					break;
				}
			default:
				/* Try our generic message handler */
				if (tw_api->defaultRequestHandler) {
					resp = tw_api->defaultRequestHandler(msg);
				} else {
					/* No handler - return a 404 */
					resp = twMessage_CreateResponseMsg(TWX_NOT_FOUND, msg->requestId);
					if (!resp) TW_LOG(TW_ERROR,"api_requesthandler: Error allocating response message");
				}
		}
	} else {
		/* Try our generic message handler */
		if (tw_api->defaultRequestHandler) {
			resp = tw_api->defaultRequestHandler(msg);
		} else {
			/* No handler - return a 404 */
			TW_LOG(TW_INFO,"api_requesthandler: No handler found.  Returning 404");
			resp = twMessage_CreateResponseMsg(TWX_NOT_FOUND, msg->requestId);
			if (!resp) TW_LOG(TW_ERROR,"api_requesthandler: Error allocating response message");
		}
	}
	/* Send our response */
	if (resp){
		res = twApi_SendResponse(resp);
		twMessage_Delete(resp);
		return res;
	}
	return TW_INVALID_RESP_MSG;
}

enum msgCodeEnum getMetadataService(const char * entityName, const char * serviceName, twInfoTable * params, twInfoTable ** content, void * userdata) {
	ListEntry * le = NULL;
	cJSON * propJson = NULL;
	cJSON * svcJson = NULL;
	cJSON * eventJson = NULL;
	cJSON * completeJson = NULL;
	char * jsonString = NULL;
	propJson = cJSON_CreateObject();
	svcJson = cJSON_CreateObject();
	eventJson = cJSON_CreateObject();
	completeJson = cJSON_CreateObject();
	TW_LOG(TW_TRACE,"getMetadataService - Function called");
	if (!content || !propJson || !svcJson || !eventJson || !completeJson ||!tw_api || !tw_api->callbackList || !entityName) {
		TW_LOG(TW_ERROR,"getMetadataService - NULL stream,callback, params or content pointer");
		if (propJson) cJSON_Delete(propJson);
		if (svcJson) cJSON_Delete(svcJson);
		if (eventJson) cJSON_Delete(eventJson);
		return TWX_BAD_REQUEST;
	}
	/* Prep the  complete JSON */
	cJSON_AddStringToObject(completeJson,"name", entityName);
	cJSON_AddStringToObject(completeJson,"description","");
	cJSON_AddFalseToObject(completeJson,"isSystemObject");
	le = twList_Next(tw_api->callbackList, NULL);
	while (le) {
		if (le->value) {
			callbackInfo * tmp = (callbackInfo *)(le->value);
			if (strcmp(entityName, tmp->entityName) == 0) {
				/* Add the definitions to the JSON */
				if (tmp->charateristicDefinition) {
					cJSON * j = cJSON_CreateObject();
					if (!j) {
						TW_LOG(TW_ERROR, "getMetadataService: Error creating JSON object");
						break;
					}
					if (tmp->characteristicType == TW_PROPERTIES && tmp->charateristicDefinition != NULL) {
						cJSON_AddStringToObject(j,"name",((twPropertyDef *)tmp->charateristicDefinition)->name);
						if (((twPropertyDef *)tmp->charateristicDefinition)->description) cJSON_AddStringToObject(j,"description",((twPropertyDef *)tmp->charateristicDefinition)->description);
						cJSON_AddStringToObject(j,"baseType",baseTypeToString(((twPropertyDef *)tmp->charateristicDefinition)->type));
						/* Add all aspects */
						cJSON_AddItemReferenceToObject(j, "aspects", ((twPropertyDef *)tmp->charateristicDefinition)->aspects);
						/* Add this porperty to the property JSON */
						cJSON_AddItemToObject(propJson, ((twPropertyDef *)tmp->charateristicDefinition)->name, j);
						le = twList_Next(tw_api->callbackList, le);
						j = NULL;
					} else if (tmp->characteristicType == TW_SERVICES && tmp->charateristicDefinition != NULL) {
						cJSON * out = NULL;
						cJSON * aspects = NULL;
						twServiceDef * svc = (twServiceDef *)tmp->charateristicDefinition;
						cJSON_AddStringToObject(j,"name", svc->name);
						cJSON_AddStringToObject(j,"description", svc->description ? svc->description : "");
						/* Add all service level aspects */
						cJSON_AddItemReferenceToObject(j, "aspects", svc->aspects);
						/* Inputs */
						if (svc->inputs) cJSON_AddItemToObject(j, "Inputs", twDataShape_ToJson(svc->inputs, NULL));
						else cJSON_AddItemToObject(j, "Inputs", cJSON_CreateObject()); /* Need the empty Inputs for Composer to parse */
						out = cJSON_CreateObject();
						aspects = cJSON_CreateObject();
						if (out && aspects) {
							cJSON_AddStringToObject(out, "baseType", baseTypeToString(svc->outputType));
							if (svc->outputDataShape) {
								/* Add the datashape name if the output is an infoTable */
								cJSON_AddStringToObject(aspects, "dataShape", svc->outputDataShape->name ? svc->outputDataShape->name : "");
								cJSON_AddItemToObject(out, "aspects", aspects);
							}
							/* Add the data shape definitions.  even if there is not datashape the fieldDefinitions elelment must exist */
							if (svc->outputDataShape) twDataShape_ToJson(svc->outputDataShape, out);
							else cJSON_AddItemToObject(out, "fieldDefinitions", cJSON_CreateObject());
							/* Data toJson function shape adds a name element at the top level - need to remove that */
							cJSON_DeleteItemFromObject(out,"name");
							/* Set the name of the output to 'result' */
							cJSON_AddStringToObject(out,"name","result");
							/* Now add the Outputs to the service definition */
							cJSON_AddItemToObject(j, "Outputs", out);
						} else {
							if (out) cJSON_Delete(out);
							if (aspects) cJSON_Delete(aspects);
						}
						/* Add this service definition to our  list of services */
						cJSON_AddItemToObject(svcJson, svc->name, j);
						le = twList_Next(tw_api->callbackList, le);
					} else if (tmp->characteristicType == TW_EVENTS && tmp->charateristicDefinition != NULL) {
						/* Event definitons are just service definitions without Outputs */
						twServiceDef * svc = (twServiceDef *)tmp->charateristicDefinition;
						cJSON * eventData = twDataShape_ToJson(svc->inputs, NULL);
						cJSON * aspects = cJSON_CreateObject();
						if (svc && eventData && aspects) {
							cJSON_AddStringToObject(j,"name", svc->name);
							cJSON_AddStringToObject(j,"description", svc->description ? svc->description : "");
							/* Add all event level aspects */
							cJSON_AddItemReferenceToObject(j, "aspects", svc->aspects);
							/* Add the Datashape name as an aspect */
							cJSON_AddStringToObject(aspects, "dataShape", svc->inputs->name ? svc->inputs->name : "");
							cJSON_AddItemToObject(eventData, "aspects", aspects);
							/* Add the dataSHape definition */
							if (svc->inputs) cJSON_AddItemToObject(j, "EventData", eventData);
							else cJSON_AddItemToObject(j, "EventData", cJSON_CreateObject()); /* Need the empty Inputs for Composer to parse */
							/* Add this event definition to our list of events */
							cJSON_AddItemToObject(eventJson, svc->name, j);
						}  else {
							if (eventData) cJSON_Delete(eventData);
							if (aspects) cJSON_Delete(aspects);
						}
						le = twList_Next(tw_api->callbackList, le);
					} else {
                            // nothing of interest, move on
                            le = twList_Next( tw_api->callbackList, le ) ;
							cJSON_Delete(j);
					} 
				} else {
                        /* No charateristic definition, move on */
                        le = twList_Next( tw_api->callbackList, le ) ;
                }
            } else {
                    /* not the value we are looking for? NEXT! */
                    le = twList_Next( tw_api->callbackList, le ) ;
            }
        } else {
                /* no eternal loops when empty value please */
                le = twList_Next( tw_api->callbackList, le ) ;
        }
	}
	/* Combine the json  */
	cJSON_AddItemToObject(completeJson, "propertyDefinitions", propJson);
	cJSON_AddItemToObject(completeJson, "serviceDefinitions", svcJson);
	cJSON_AddItemToObject(completeJson, "eventDefinitions", eventJson);
	/* Create the result infotable */
	jsonString = cJSON_PrintUnformatted(completeJson);
	*content = twInfoTable_CreateFromPrimitive("result", twPrimitive_CreateFromVariable(jsonString, TW_JSON, FALSE, 0));
	cJSON_Delete(completeJson);
	if (*content) {
		return TWX_SUCCESS;
	}
	return TWX_INTERNAL_SERVER_ERROR;
}

char receivedPong = FALSE;
int pong_handler (struct twWs * ws, const char * data, size_t length) {
	receivedPong = TRUE;
	return 0;
}

int makeAuthOrBindCallbacks(char * entityName, enum entityTypeEnum entityType, char type, char * value) { 
	/* Type -> 0 - Auth, 1 - Bind, 2 - Unbind */
	ListEntry * le = NULL;
	/* Validate all inputs */
	if ((type == 0 && (value == NULL || entityName == NULL)) || !tw_api || !tw_api->bindEventCallbackList) {
		TW_LOG(TW_ERROR, "makeAuthOrBindCallback: Invalid parameter found");
		return TW_INVALID_PARAM;
	}
	le = twList_Next(tw_api->bindEventCallbackList, NULL);
	while (le && le->value) {
		callbackInfo * info = (callbackInfo *)le->value;
		if (type == 0) {
			/* Auth callback */
			if (info->entityType == TW_APPLICATIONKEYS) {
				authEvent_cb cb = (authEvent_cb)info->cb;
				cb(entityName, value, info->userdata);
			}
		} else {
			if (info->entityType != TW_APPLICATIONKEYS && (!entityName || (!info->entityName || !strcmp(info->entityName, entityName)))) {
				bindEvent_cb cb = (bindEvent_cb)info->cb;
				cb(info->entityName, (type == 1), info->userdata);
			}
		}
		le = twList_Next(tw_api->bindEventCallbackList, le);
	}
	return TW_OK;
}

int registerServiceOrEvent(enum entityTypeEnum entityType, char * entityName, char * serviceName, char * serviceDescription,
						  twDataShape * inputs, enum BaseType outputType, twDataShape * outputDataShape, service_cb cb, void * userdata, char isService) {
	callbackInfo * info = NULL;
	if (tw_api && tw_api->callbackList && entityName && serviceName && ((isService && cb) || !isService)) {
		twServiceDef * service = twServiceDef_Create(serviceName, serviceDescription, inputs, outputType, 
														outputDataShape);
		info = (callbackInfo *)TW_CALLOC(sizeof(callbackInfo), 1);
		if (!info || !service) {
			TW_LOG(TW_ERROR, "registerServiceOrEvent: Error allocating callback info");
			if (info) TW_FREE(info);
			twServiceDef_Delete(service);
			return TW_ERROR_ALLOCATING_MEMORY;
		}
		info->entityType = entityType;
		info->entityName = duplicateString(entityName);
		info->characteristicType = isService ? TW_SERVICES : TW_EVENTS;
		info->charateristicName = duplicateString(service->name);
		info->charateristicDefinition = service;
		info->cb = cb;
		info->userdata = userdata;
		return twList_Add(tw_api->callbackList, info);
	}
	TW_LOG(TW_ERROR, "registerServiceOrEvent: Invalid params or missing api pointer");
	return TW_INVALID_PARAM;
}

int AddAspectToEntity(char * entityName, enum characteristicEnum type,  char * characteristicName,  
							  char * aspectName, twPrimitive * aspectValue) {
	if (tw_api && tw_api->callbackList && entityName && characteristicName && aspectName && aspectValue) {
		/* Find the characteristic */
		ListEntry * le = NULL;
		le = twList_Next(tw_api->callbackList, NULL);
		while (le && le->value) {
			callbackInfo * cb = (callbackInfo *)le->value;
			if (cb->characteristicType == type) {
				if (!strcmp(cb->entityName, entityName) && !strcmp(cb->charateristicName, characteristicName)) {
					switch (type) {
					case TW_PROPERTIES:
						{
						twPropertyDef * def = (twPropertyDef *)cb->charateristicDefinition;
						twPrimitive_ToJson(aspectName, aspectValue, def->aspects);
						break;
						}
					case TW_SERVICES:
					case TW_EVENTS:
						{
						twServiceDef * def = (twServiceDef *)cb->charateristicDefinition;
						twPrimitive_ToJson(aspectName, aspectValue, def->aspects);
						break;
						}
					default:
						break;
					}
					twPrimitive_Delete(aspectValue);
					return TW_OK;
				}
			}
			le = twList_Next(tw_api->callbackList, le);
		}
		TW_LOG(TW_ERROR, "AddAspectToEntity: CHarateristic %s not found in Entity %s", characteristicName, entityName);
	}
	if (aspectValue) twPrimitive_Delete(aspectValue);
	TW_LOG(TW_ERROR, "twApi_AddAspectToProperty: Invalid params or missing api pointer");
	return TW_INVALID_PARAM;
}

/*********************************************************/
/* API Functions */
int twApi_Initialize(char * host, uint16_t port, char * resource, char * app_key, char * gatewayName,
						 uint32_t messageChunkSize, uint16_t frameSize, char autoreconnect) {
	int err = TW_UNKNOWN_ERROR;
	int retries = twcfg.connect_retries;
    twWs * ws = NULL;
	/* Validate all inputs */
	if (!host || port == 0 || !resource || !app_key || messageChunkSize < frameSize) {
		TW_LOG(TW_ERROR, "twApi_Initialize: Invalid parameter found");
		return TW_INVALID_PARAM;
	}
	/* Create our global initialization mutex */
	if (!twInitMutex) twInitMutex = twMutex_Create();
	if (!twInitMutex) {
		TW_LOG(TW_ERROR, "twApi_Initialize: Error creating initialization mutex");
		return TW_ERROR_CREATING_MTX;
	}
	twMutex_Lock(twInitMutex);
	/* Check to see if the singleton already exists */
	if (tw_api) {
		TW_LOG(TW_WARN, "twApi_Initialize: API singleton already exists");
		twMutex_Unlock(twInitMutex);
		return TW_OK;
	}
	/* Create the websocket */
	err = twWs_Create(host, port, resource, app_key, gatewayName, messageChunkSize, frameSize, &ws);
	while (retries !=0 && err) {
		TW_LOG(TW_ERROR, "twApi_Initialize: Error creating websocket structure, retrying");
		twSleepMsec(twcfg.connect_retry_interval);
		if (retries > 0) {
			retries--;
		}
		err = twWs_Create(host, port, resource, app_key, gatewayName, messageChunkSize, frameSize, &ws);
	}
	if (err) {
		TW_LOG(TW_ERROR, "twApi_Initialize: Could not connect after %d attempts", twcfg.connect_retries);
		twMutex_Unlock(twInitMutex);
		return err;
	}
	TW_LOG(TW_DEBUG, "twApi_Initialize: Websocket Established after %d tries", (twcfg.connect_retries - retries));
	
	/* Allocate space for the structure */
	tw_api = (twApi *)TW_CALLOC(sizeof(twApi), 1);
	if (!tw_api) {
		twWs_Delete(ws);
		TW_LOG(TW_ERROR, "twApi_Initialize: Error allocating api structure");
		twMutex_Unlock(twInitMutex);
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	/* Initialize the message handler, mutex, and lists */
	tw_api->mh = twMessageHandler_Instance(ws);
	tw_api->mtx = twMutex_Create();
	tw_api->callbackList = twList_Create(deleteCallbackInfo);
	tw_api->bindEventCallbackList = twList_Create(deleteCallbackInfo);
	tw_api->boundList = twList_Create(bindListEntry_Delete);
	if (!tw_api->mh || !tw_api->mtx || !tw_api->callbackList || !tw_api->boundList || !tw_api->bindEventCallbackList) {
		TW_LOG(TW_ERROR, "twApi_Initialize: Error initializing api");
		twApi_Delete();
		twMutex_Unlock(twInitMutex);
		return TW_ERROR_INITIALIZING_API;
	}
	twMessageHandler_RegisterDefaultRequestCallback(tw_api->mh, api_requesthandler);
	tw_api->autoreconnect = autoreconnect;
	tw_api->manuallyDisconnected = TRUE;
	tw_api->isAuthenticated = FALSE;
	tw_api->defaultRequestHandler = NULL;
	tw_api->connectionInProgress = FALSE;
	twApi_SetDutyCycle(twcfg.duty_cycle, twcfg.duty_cycle_period);
	/* Set up our ping/pong handling */
	tw_api->ping_rate = twcfg.ping_rate;
	tw_api->handle_pongs = TRUE;
	twMessageHandler_RegisterPongCallback(tw_api->mh, pong_handler);

#ifdef OFFLINE_MSG_STORE
	tw_api->offlineMsgEnabled = TRUE;
#if (OFFLINE_MSG_STORE == 1) 
	tw_api->offlineMsgList = twList_Create(twStream_Delete);
	if (!tw_api->offlineMsgList) {
		TW_LOG(TW_ERROR, "twApi_Initialize: Error allocating offline message store queue");
	}
#endif
#if (OFFLINE_MSG_STORE == 2)
	if (twApi_SetOfflineMsgStoreDir(twcfg.offline_msg_store_dir)) {
		TW_LOG(TW_ERROR, "twApi_Initialize: Error creating offline message directory %s", twcfg.offline_msg_store_dir ? twcfg.offline_msg_store_dir : "NULL");
	}
#endif
#endif
#ifdef ENABLE_TASKER
	/* Initalize our tasker */
	twTasker_CreateTask(5, &twApi_TaskerFunction);
	twTasker_Start();
#endif
#ifdef ENABLE_TUNNELING
	/* Create our connection info structure */
	tw_api->connectionInfo = twConnectionInfo_Create(NULL);
	if (tw_api->connectionInfo) {
		tw_api->connectionInfo->ws_host = duplicateString(host);
		tw_api->connectionInfo->ws_port = port;
		tw_api->connectionInfo->appkey = duplicateString(app_key);
		twTunnelManager_Create();
	}
#endif

	/* Initialize the Subscribed Properties Manager */
	err = twSubscribedPropsMgr_Initialize();
	if (err) { 
		TW_LOG(TW_ERROR, "twApi_Initialize: Error initializing api");
		twApi_Delete();
		twMutex_Unlock(twInitMutex);
		return TW_ERROR_INITIALIZING_API;
	}
	twMutex_Unlock(twInitMutex);
	return TW_OK;
}
	
int twApi_Delete() {
	twApi * tmp = tw_api;
	if (!tw_api) return TW_OK;
	twApi_Disconnect("Shutting down");
	twApi_StopConnectionAttempt();
#ifdef ENABLE_TASKER
	/* Stop the tasker */
	twTasker_Stop();
#endif
#ifdef ENABLE_FILE_XFER
	twFileManager_Delete();
#endif
#ifdef ENABLE_TUNNELING
	twTunnelManager_Delete();
#endif
	if (tw_api->connectionInfo) twConnectionInfo_Delete(tw_api->connectionInfo);
	/* Shut down the subscribed property Manager */
	twSubscribedPropsMgr_Delete();
	/* Set the singleton to NULL so no one else uses it */
	twMutex_Lock(twInitMutex);
	twMutex_Lock(tmp->mtx);	
	tw_api = NULL;
	if (tmp->mh) twMessageHandler_Delete(NULL);
	if (tmp->callbackList) twList_Delete(tmp->callbackList);
	if (tmp->bindEventCallbackList) twList_Delete(tmp->bindEventCallbackList);
	if (tmp->boundList) twList_Delete(tmp->boundList);
	if (tmp->offlineMsgList) twList_Delete(tmp->offlineMsgList);
	if (tmp->offlineMsgFile) TW_FREE(tmp->offlineMsgFile);
	if (tmp->subscribedPropsFile)TW_FREE(tmp->subscribedPropsFile);
    twMutex_Unlock(tmp->mtx);
	twMutex_Delete(tmp->mtx);
	twMutex_Unlock(twInitMutex);
	twMutex_Delete(twInitMutex);
	twInitMutex = NULL;
	TW_FREE(tmp);
	twLogger_Delete();
	return TW_OK;
}

#ifdef ENABLE_HTTP_PROXY_SUPPORT
int twApi_SetProxyInfo(char * proxyHost, uint16_t proxyPort, char * proxyUser, char * proxyPass) {
	if (!tw_api || !tw_api->mh || !tw_api->mh->ws || !tw_api->mh->ws->connection || !tw_api->mh->ws->connection->connection) return TW_SOCKET_INIT_ERROR;
	return twSocket_SetProxyInfo(tw_api->mh->ws->connection->connection, proxyHost, proxyPort, proxyUser, proxyPass);
}

int twApi_ClearProxyInfo() {
	if (!tw_api || !tw_api->mh || !tw_api->mh->ws || !tw_api->mh->ws->connection || !tw_api->mh->ws->connection->connection) return TW_SOCKET_INIT_ERROR;
	return twSocket_ClearProxyInfo(tw_api->mh->ws->connection->connection);
}
#endif

char * twApi_GetVersion() {
	return C_SDK_VERSION;
}

int twApi_BindAll(char unbind) {
	enum msgCodeEnum res = TWX_SUCCESS;
	twMessage * msg = NULL;
	ListEntry * le = NULL;
	if (!tw_api || !tw_api->mh || !tw_api->mh->ws || !tw_api->boundList) return TW_INVALID_PARAM;
	twMutex_Lock(tw_api->mtx);
	le = twList_Next(tw_api->boundList, NULL);
	if (le && le->value) {
		bindListEntry * e = (bindListEntry *)le->value;
		msg = twMessage_CreateBindMsg(e->name, unbind);
		while (le && le->value) {
			le = twList_Next(tw_api->boundList, le);
			if (le && le->value) {
				e = (bindListEntry *)le->value;
				twBindBody_AddName((twBindBody *)(msg->body), e->name);
			}
		}
		res = sendMessageBlocking(msg, twcfg.default_message_timeout, NULL);
	}
	twMutex_Unlock(tw_api->mtx);
	if (msg) twMessage_Delete(msg);
	/* Look for any callbacks */
	if (res == TWX_SUCCESS) makeAuthOrBindCallbacks(NULL, TW_THING, 1, NULL); 
	return convertMsgCodeToErrorCode(res);
}

int twApi_Authenticate() {
	int res = TW_UNKNOWN_ERROR;
	twMessage * msg = NULL;
	if (!tw_api || !tw_api->mh || !tw_api->mh->ws) return TW_INVALID_PARAM;
	twMutex_Lock(tw_api->mtx);
	msg = twMessage_CreateAuthMsg("appKey", tw_api->mh->ws->api_key);
	res = convertMsgCodeToErrorCode(sendMessageBlocking(msg, twcfg.default_message_timeout, NULL));
	if (res == TW_OK) tw_api->isAuthenticated = TRUE;
	twMutex_Unlock(tw_api->mtx);
	if (msg) twMessage_Delete(msg);
	return res;
}

int twApi_Connect(uint32_t timeout, int32_t retries) {
	int res = TW_OK;
	if (!tw_api) return TW_NULL_API_SINGLETON;
	tw_api->connect_timeout = timeout;
	tw_api->connect_retries = retries;
	if (tw_api->mh && tw_api->mh->ws && !tw_api->connectionInProgress) {
		uint32_t delayTime;
		tw_api->connectionInProgress = TRUE;
		tw_api->manuallyDisconnected = TRUE;
		// Delay a random amount
		delayTime = (rand() * twcfg.max_connect_delay)/RAND_MAX;
		TW_LOG(TW_TRACE, "twApi_Connect: Delaying %d milliseconds before connecting", delayTime);
		twSleepMsec(delayTime);
		while (retries != 0 && tw_api->connectionInProgress) {
			twMutex_Lock(tw_api->mtx);
			res = twWs_Connect(tw_api->mh->ws, timeout);
			/*   if (res == TW_OK) tw_api->manuallyDisconnected = FALSE;  */
			twMutex_Unlock(tw_api->mtx);
			if (!res) res = twApi_Authenticate();
			if (!res) res = twApi_BindAll(FALSE);
			if (!res) break;
			if (retries != -1) retries--;
			twSleepMsec(twcfg.connect_retry_interval);
		}
		if (retries == 0) {
			/* if retries hits 0, that means we have attempted connecting the max number of times and we should warn the user */
			TW_LOG(TW_ERROR, "twApi_Connect: Max number of connect retries: %d, has been reached", tw_api->connect_retries);
		}
		/* 
		If we have exhausted out retries we don't want to continue
		attempting to connect
		*/
		if (res == TW_OK) {
			tw_api->manuallyDisconnected = FALSE;
		}
		tw_api->connectionInProgress = FALSE;
	}
	tw_api->firstConnectionComplete = TRUE;
	return res;
}

int twApi_Disconnect(char * reason) {
	int res = TW_UNKNOWN_ERROR;
	if (!tw_api) return TW_NULL_API_SINGLETON;
	twApi_BindAll(TRUE);
	twMutex_Lock(tw_api->mtx);
	if (tw_api->mh){
		tw_api->manuallyDisconnected = TRUE;
		tw_api->isAuthenticated = FALSE;
		res = twWs_Disconnect(tw_api->mh->ws, NORMAL_CLOSE, reason);
	}
	
	twMutex_Unlock(tw_api->mtx);
	return res;
}

char twApi_isConnected() {
	if (tw_api && tw_api->mh && tw_api->mh->ws) return twWs_IsConnected(tw_api->mh->ws) && tw_api->isAuthenticated;
	return FALSE;
}

char twApi_ConnectionInProgress() {
	if (tw_api) return tw_api->connectionInProgress;
	return FALSE;
}

int twApi_StopConnectionAttempt() {
	if (tw_api) tw_api->connectionInProgress = FALSE;
	return 0;
}

int twApi_SetDutyCycle(uint8_t duty_cycle, uint32_t period) {
	if (!tw_api) return TW_NULL_API_SINGLETON;
	if (duty_cycle > 100) duty_cycle = 100;
	tw_api->duty_cycle = duty_cycle;
	tw_api->duty_cycle_period = period;
	twcfg.duty_cycle = duty_cycle;
	twcfg.duty_cycle_period = period;
	return TW_OK;
}

int twApi_SetPingRate(uint32_t rate) {
	if (!tw_api) return TW_NULL_API_SINGLETON;
	tw_api->ping_rate = rate;
	return TW_OK;
}

int twApi_SetConnectTimeout(uint32_t timeout) {
	if (!tw_api) return TW_NULL_API_SINGLETON;
	tw_api->connect_timeout = timeout;
	return TW_OK;
}

int twApi_SetConnectRetries(signed char retries) {
	if (!tw_api) return TW_NULL_API_SINGLETON;
	tw_api->connect_retries = retries;
	return TW_OK;
}

int twApi_SetGatewayName(const char* input_name){
	if(!input_name) return TW_ERROR;
	tw_api->mh->ws->gatewayName = duplicateString(input_name);
	if(!tw_api->mh->ws->gatewayName) return TW_ERROR;
	return TW_OK;
}

int twApi_SetGatewayType(const char* input_type){
	if(!input_type) return TW_ERROR;
	tw_api->mh->ws->gatewayType = duplicateString(input_type);
	if(!tw_api->mh->ws->gatewayType) return TW_ERROR;
	return TW_OK;
}


int twApi_BindThing(char * entityName) {
	int res = TW_UNKNOWN_ERROR;
	enum msgCodeEnum resp;
	twMessage * msg = NULL;
	if (!tw_api || !entityName || !tw_api->bindEventCallbackList) {
		TW_LOG(TW_ERROR, "twApi_BindThing: NULL tw_api or entityName");
		return TW_INVALID_PARAM;
	}
	/* Add it to the list */
	twList_Add(tw_api->boundList, bindListEntry_Create(entityName));
	/* Register our metadata service handler */
	twApi_RegisterService(TW_THING, entityName, "GetMetadata", NULL, NULL, TW_JSON, NULL, getMetadataService, NULL);
	twApi_RegisterService(TW_THING, entityName, "NotifyPropertyUpdate", NULL, NULL, TW_NOTHING, NULL, notifyPropertyUpdateHandler, NULL);
	/* If we are not connected, we are done */
	if (!twApi_isConnected()) return TW_OK;
	/* Create the bind message */
	msg = twMessage_CreateBindMsg(entityName, FALSE);
	if (!msg) {
		TW_LOG(TW_ERROR, "twApi_BindThing: Error creating Bind message");
		return TW_ERROR_CREATING_MSG;
	}
	twMutex_Lock(tw_api->mtx);
	resp = sendMessageBlocking(msg, 10000, NULL);
	twMutex_Unlock(tw_api->mtx);
	res = convertMsgCodeToErrorCode(resp);
	if (res != TW_OK) TW_LOG(TW_ERROR, "twApi_BindThing: Error sending Bind message");
	if (msg) twMessage_Delete(msg);
	/* Look for any callbacks */
	if (!res) makeAuthOrBindCallbacks(entityName, TW_THING, 1, NULL); 
	return res;
}

int twApi_UnbindThing(char * entityName) {
	int res = TW_UNKNOWN_ERROR;
	twMessage * msg = NULL;
	enum msgCodeEnum resp;
	ListEntry * le = NULL;
	if (!tw_api || !entityName || !tw_api->boundList) {
		TW_LOG(TW_ERROR, "twApi_UnbindThing: NULL tw_api or entityName");
		return TW_INVALID_PARAM;
	}
	/* Unregister all call backs for this entity */
    twApi_UnregisterThing(entityName);
	/* Remove it from the Bind list */
	le = twList_Next(tw_api->boundList, NULL);
	while (le) {
		bindListEntry * e = (bindListEntry *)le->value;
		if (le->value && !strcmp(entityName, e->name)) {
			twList_Remove(tw_api->boundList, le, TRUE);
			break;
		}
		le = twList_Next(tw_api->boundList, le);
	}
	/* Create the bind message */
	msg = twMessage_CreateBindMsg(entityName, TRUE);
	if (!msg) {
		TW_LOG(TW_ERROR, "twApi_UnbindThing: Error creating Unbind message");
		return TW_ERROR_CREATING_MSG;
	}
	twMutex_Lock(tw_api->mtx);
	resp = sendMessageBlocking(msg, 10000, NULL);
	twMutex_Unlock(tw_api->mtx);
	res = convertMsgCodeToErrorCode(resp);
	if (res != TW_OK) TW_LOG(TW_ERROR, "twApi_UnbindThing: Error creating sending Unbind message");
	if (msg) twMessage_Delete(msg);
	/* Look for any callbacks */
	if (!res) makeAuthOrBindCallbacks(entityName, TW_THING, 2, NULL); 
	return res;
}

char twApi_IsEntityBound(char * entityName) {
	ListEntry * le = NULL;
	if (!tw_api || !entityName || !tw_api->boundList) {
		TW_LOG(TW_ERROR, "twApi_IsEntityBound: NULL tw_api or entityName");
		return FALSE;
	}
	le = twList_Next(tw_api->boundList, NULL);
	while (le) {
		bindListEntry * e = (bindListEntry *)le->value;
		if (le->value && !strcmp(entityName, e->name)) {
			return TRUE;
		}
		le = twList_Next(tw_api->boundList, le);
	}
	return FALSE;
}

void twApi_TaskerFunction(DATETIME now, void * params) {
	static DATETIME nextPingTime;
	static DATETIME expectedPongTime;
	static DATETIME nextCleanupTime;
	static DATETIME nextUpdatePropertyCheckTime;
	static DATETIME nextDutyCycleEvent;
	static char madeAuthCallbacks;
	/* This is the main "loop" of the api */
	if (tw_api && tw_api->mh && tw_api->mh->ws) {
		if (twApi_isConnected()) {
			if (twTimeGreaterThan(now, nextPingTime)) {
				/* Time to send our keep alive ping */
				expectedPongTime = twAddMilliseconds(now, twcfg.pong_timeout);
				receivedPong = FALSE;
				twWs_SendPing(tw_api->mh->ws,0);
				nextPingTime = twAddMilliseconds(now, tw_api->ping_rate);
			}
			if (tw_api->handle_pongs && !receivedPong && twTimeGreaterThan(now, expectedPongTime)) {
				/* We didn't receive a pong in time */
				TW_LOG(TW_WARN,"apiThread: Did not receive pong in time");
				tw_api->manuallyDisconnected = FALSE;
				tw_api->isAuthenticated = FALSE;
				/* This call will likely fail since we are already not connected, but do it just in case */
				twWs_Disconnect(tw_api->mh->ws, NORMAL_CLOSE, "Pong timeout");
				/* Force a ping next time we connect */
				nextPingTime = 0;
			}
			subscribedPropertyUpdateTask(now, NULL);
		}
		if (twTimeGreaterThan(now, nextCleanupTime)) {
			twMessageHandler_CleanupOldMessages(tw_api->mh);
			twMultipartMessageStore_RemoveStaleMessages();
			nextCleanupTime = twAddMilliseconds(now, twcfg.stale_msg_cleanup_rate);
		}
		if (twTimeGreaterThan(now, nextUpdatePropertyCheckTime)) {
			subscribedPropertyUpdateTask(now, NULL);
			nextUpdatePropertyCheckTime = twAddMilliseconds(now, 500);
		}
		if (tw_api->firstConnectionComplete && tw_api->duty_cycle_period && tw_api->duty_cycle < 100 && twTimeGreaterThan(now, nextDutyCycleEvent)) {
			if (twApi_isConnected()) {
				TW_LOG(TW_INFO,"apiThread: Entering Duty Cycle OFF state.");
				twApi_Disconnect("Duty cycle off time");
				nextDutyCycleEvent = twAddMilliseconds(now, (tw_api->duty_cycle_period * (100 - tw_api->duty_cycle))/100);
			}  else {
				TW_LOG(TW_INFO,"apiThread: Entering Duty Cycle ON state.");
				twApi_Connect(tw_api->connect_timeout, tw_api->connect_retries);
				nextDutyCycleEvent = twAddMilliseconds(now, (tw_api->duty_cycle_period * tw_api->duty_cycle)/100);
			}
		}
		if (tw_api && tw_api->mh && tw_api->mh->ws && !tw_api->mh->ws->isConnected) {
			tw_api->isAuthenticated = FALSE;
			madeAuthCallbacks = FALSE;
		}
		if (tw_api->isAuthenticated && !madeAuthCallbacks) {
			makeAuthOrBindCallbacks("appKey", TW_APPLICATIONKEYS, 0, tw_api->mh->ws->api_key);
			madeAuthCallbacks = TRUE;
		}
		if (!twWs_IsConnected(tw_api->mh->ws) && tw_api && tw_api->autoreconnect && tw_api->manuallyDisconnected == FALSE) {
			twApi_Connect(tw_api->connect_timeout, tw_api->connect_retries);
		}
	}
	if (tw_api && twWs_IsConnected(tw_api->mh->ws) && tw_api->manuallyDisconnected == FALSE) {
		twWs_Receive(tw_api->mh->ws, twcfg.socket_read_timeout);
	}
}

int twApi_RegisterProperty(enum entityTypeEnum entityType, char * entityName, char * propertyName, enum BaseType propertyType, 
						   char * propertyDescription, char * propertyPushType, double propertyPushThreshold, property_cb cb, void * userdata) {
	callbackInfo * info = NULL;
	if (tw_api && tw_api->callbackList && entityName && propertyName && cb) {
		twPropertyDef * property = twPropertyDef_Create(propertyName, propertyType, propertyDescription, propertyPushType, propertyPushThreshold);
		info = (callbackInfo *)TW_CALLOC(sizeof(callbackInfo), 1);
		if (!info || !property) {
			TW_LOG(TW_ERROR, "twApi_RegisterProperty: Error allocating callback info");
			twPropertyDef_Delete(property);
			if (info) TW_FREE(info);
			return TW_ERROR_ALLOCATING_MEMORY;
		}
		info->entityType = entityType;
		info->entityName = duplicateString(entityName);
		info->characteristicType = TW_PROPERTIES;
		info->charateristicName = duplicateString(property->name);
		info->charateristicDefinition = property;
		info->cb = cb;
		info->userdata = userdata;
		return (twList_Add(tw_api->callbackList, info));
	}
	TW_LOG(TW_ERROR, "twApi_RegisterProperty: Invalid params or missing api pointer");
	return TW_INVALID_PARAM;
}

int twApi_UpdatePropertyMetaData(enum entityTypeEnum entityType, char * entityName, char * propertyName, enum BaseType propertyType, 
									char * propertyDescription, char * propertyPushType, double propertyPushThreshold) {

	callbackInfo * info = NULL;
	twPropertyDef * tmp = NULL;
	ListEntry * le = findRegisteredItem(tw_api->callbackList, entityType, entityName, TW_PROPERTIES, propertyName);
	if (!le || !le->value) {
		TW_LOG(TW_ERROR, "twApi_UpdatePropertyMetaData: Could not find property %s:%s", entityName ? entityName : "NULL", propertyName ? propertyName : "NULL");
		return TW_INVALID_PARAM;
	}
	info = (callbackInfo *)le->value;
	tmp = twPropertyDef_Create(propertyName, 
								(propertyType == TW_UNKNOWN_TYPE) ? ((twPropertyDef *)info->charateristicDefinition)->type : propertyType,
								propertyDescription ? propertyDescription : ((twPropertyDef *)info->charateristicDefinition)->description, 
								propertyPushType, 
								propertyPushThreshold);
	if (!tmp) {
		TW_LOG(TW_ERROR, "twApi_UpdatePropertyMetaData: Error creating property definition");
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	twPropertyDef_Delete(info->charateristicDefinition);
	info->charateristicDefinition = tmp;
	return TW_OK;
}

int twApi_AddAspectToProperty(char * entityName, char * propertyName,  
							  char * aspectName, twPrimitive * aspectValue) {
	return AddAspectToEntity(entityName, TW_PROPERTIES, propertyName, aspectName, aspectValue);
}

int twApi_RegisterService(enum entityTypeEnum entityType, char * entityName, char * serviceName, char * serviceDescription,
						  twDataShape * inputs, enum BaseType outputType, twDataShape * outputDataShape, service_cb cb, void * userdata) {

    return registerServiceOrEvent(entityType, entityName, serviceName, serviceDescription, inputs, outputType, outputDataShape, cb, userdata, TRUE);
}

int twApi_AddAspectToService(char * entityName, char * serviceName,  
							  char * aspectName, twPrimitive * aspectValue) {
	return AddAspectToEntity(entityName, TW_SERVICES, serviceName, aspectName, aspectValue);
}

int twApi_RegisterEvent(enum entityTypeEnum entityType, char * entityName, char * eventName, char * eventDescription, twDataShape * parameters) {
	
	return registerServiceOrEvent(entityType, entityName, eventName, eventDescription, parameters, TW_NOTHING, NULL, NULL, NULL, FALSE);
}

int twApi_AddAspectToEvent(char * entityName, char * eventName,  
							  char * aspectName, twPrimitive * aspectValue) {
	return AddAspectToEntity(entityName, TW_EVENTS, eventName, aspectName, aspectValue);
}

int twApi_RegisterPropertyCallback(enum entityTypeEnum entityType, char * entityName, char * propertyName, property_cb cb, void * userdata) {
	callbackInfo * info = NULL;
	if (tw_api && tw_api->callbackList && entityName && propertyName && cb) {
		info = (callbackInfo *)TW_CALLOC(sizeof(callbackInfo), 1);
		if (!info) {
			TW_LOG(TW_ERROR, "twApi_RegisterPropertyCallback: Error allocating callback info");
			return TW_ERROR_ALLOCATING_MEMORY;
		}
		info->entityType = entityType;
		info->entityName = duplicateString(entityName);
		info->characteristicType = TW_PROPERTIES;
		info->charateristicName = duplicateString(propertyName);
		info->charateristicDefinition = NULL;
		info->cb = cb;
		info->userdata = userdata;
		return (twList_Add(tw_api->callbackList, info));
	}
	TW_LOG(TW_ERROR, "twApi_RegisterPropertyCallback: Invalid params or missing api pointer");
	return TW_INVALID_PARAM;
}

int twApi_RegisterServiceCallback(enum entityTypeEnum entityType, char * entityName, char * serviceName, service_cb cb, void * userdata) {
	callbackInfo * info = NULL;
	if (tw_api && tw_api->callbackList && entityName && serviceName && cb) {
		info = (callbackInfo *)TW_CALLOC(sizeof(callbackInfo), 1);
		if (!info) {
			TW_LOG(TW_ERROR, "twApi_RegisterServiceCallback: Error allocating callback info");
			return TW_ERROR_ALLOCATING_MEMORY;
		}
		info->entityType = entityType;
		info->entityName = duplicateString(entityName);
		info->characteristicType = TW_SERVICES;
		info->charateristicName = duplicateString(serviceName);
		info->charateristicDefinition = NULL;
		info->cb = cb;
		info->userdata = userdata;
		return twList_Add(tw_api->callbackList, info);
	}
	TW_LOG(TW_ERROR, "twApi_RegisterServiceCallback: Invalid params or missing api pointer");
	return TW_INVALID_PARAM;
}

int twApi_UnregisterThing(char * entityName) {
	if (tw_api && tw_api->callbackList && entityName) {
		/* Get all callbacks registered to this entity */
		ListEntry * le = NULL;
		le = twList_Next(tw_api->callbackList, NULL);
		while (le) {
			if (le->value) {
				callbackInfo * tmp = (callbackInfo *)(le->value);
				if (strcmp(entityName, tmp->entityName)) {
					le = twList_Next(tw_api->callbackList, le);
					continue;
				}
				/* Delete this entry */
				twList_Remove(tw_api->callbackList, le, TRUE);
				le = twList_Next(tw_api->callbackList, le->prev);
			}
		}
		return 0;
	}
	TW_LOG(TW_ERROR, "twApi_UnregisterThing: Invalid params or missing api pointer");
	return TW_INVALID_PARAM;
}

int twApi_UnregisterCallback(char * entityName, enum characteristicEnum type, char * characteristicName, void * userdata) {
	if (tw_api && tw_api->callbackList && entityName && characteristicName) {
		/* Get all callbacks registered to this entity */
		ListEntry * le = NULL;
		le = twList_Next(tw_api->callbackList, NULL);
		while (le) {
			if (le->value) {
				callbackInfo * tmp = (callbackInfo *)(le->value);
				if (strcmp(entityName, tmp->entityName) || tmp->characteristicType != type || strcmp(characteristicName, tmp->charateristicName) || tmp->userdata != userdata ) {
					le = twList_Next(tw_api->callbackList, le);
					continue;
				}
				/* Delete this entry */
				twList_Remove(tw_api->callbackList, le, TRUE);
				le = twList_Next(tw_api->callbackList, le->prev);
			}
		}
		return 0;
	}
	TW_LOG(TW_ERROR, "twApi_UnregisterCallback: Invalid params or missing api pointer");
	return TW_INVALID_PARAM;
}

int twApi_UnregisterPropertyCallback(char * entityName, char * propertyName, void * cb) {
	return twApi_UnregisterCallback(entityName, TW_PROPERTIES, propertyName, cb);
}

int twApi_UnregisterServiceCallback(char * entityName, char * serviceName, void * cb) {
	return twApi_UnregisterCallback(entityName, TW_SERVICES, serviceName, cb);
}

int twApi_RegisterDefaultRequestHandler(genericRequest_cb cb) {
	if (tw_api) {
		tw_api->defaultRequestHandler = cb;
		return TW_OK;
	}
	return TW_NULL_API_SINGLETON;
}

propertyList * twApi_CreatePropertyList(char * name, twPrimitive * value, DATETIME timestamp) {
	propertyList * proplist = twList_Create(twProperty_Delete);
	if (!proplist) {
		TW_LOG(TW_ERROR,"twApi_CreatePropertyList: Error allocating property list");
		return NULL;
	}
	if (twList_Add(proplist, twProperty_Create(name, value, timestamp))) {
		TW_LOG(TW_ERROR,"twApi_CreatePropertyList: Error adding initial property  to list");
		twList_Delete(proplist);
		return NULL;
	}
	return proplist;
}

int twApi_DeletePropertyList(propertyList * list) {
	return twList_Delete(list);
}

int twApi_AddPropertyToList(propertyList * proplist, char * name, twPrimitive * value, DATETIME timestamp) {
	return twList_Add(proplist, twProperty_Create(name, value, timestamp));
}

int twApi_ReadProperty(enum entityTypeEnum entityType, char * entityName, char * propertyName, twPrimitive ** result, int32_t timeout, char forceConnect) {
	enum msgCodeEnum res = makePropertyRequest(TWX_GET, entityType, entityName, propertyName, 0, result, timeout, forceConnect);
	return convertMsgCodeToErrorCode(res);
}

int twApi_WriteProperty(enum entityTypeEnum entityType, char * entityName, char * propertyName, twPrimitive * value, int32_t timeout, char forceConnect) {
	twPrimitive * result = NULL;
	enum msgCodeEnum res;
	res = makePropertyRequest(TWX_PUT, entityType, entityName, propertyName, value, &result, timeout, forceConnect);
	twPrimitive_Delete(result);
	return convertMsgCodeToErrorCode(res);
}

int twApi_SetSubscribedPropertyVTQ(char * entityName, char * propertyName, twPrimitive * value,  DATETIME timestamp, char * quality, char fold, char pushUpdate) {
	return twSubscribedPropsMgr_SetPropertyVTQ(entityName, propertyName, value, timestamp, quality, fold, pushUpdate);
}

int twApi_SetSubscribedProperty(char * entityName, char * propertyName, twPrimitive * value, char fold, char pushUpdate) {
	return twSubscribedPropsMgr_SetPropertyVTQ(entityName, propertyName, value, twGetSystemTime(TRUE), "GOOD", fold, pushUpdate);
}

int twApi_PushSubscribedProperties(char * entityName, char forceConnect) {
	return twSubscribedPropsMgr_PushSubscribedProperties(entityName, forceConnect);
}

int twApi_PushProperties(enum entityTypeEnum entityType, char * entityName, propertyList * properties, int32_t timeout, char forceConnect) {
	twDataShape* ds = NULL;
	twInfoTable * it = NULL;
	twInfoTable * values = NULL;
	twInfoTable * result = NULL;
	ListEntry * le = NULL;
	int res = TW_UNKNOWN_ERROR;
	/* Validate the inoputs */
	if (!entityName || !properties) {
		TW_LOG(TW_ERROR,"twApi_PushProperties: Missing inputs");
		return TWX_BAD_REQUEST;
	}
	/* Create the data shape */
	ds = twDataShape_Create(twDataShapeEntry_Create("name", NULL, TW_STRING));
	if (!ds) {
		TW_LOG(TW_ERROR,"twApi_PushProperties: Error allocating data shape");
		return TWX_INTERNAL_SERVER_ERROR;
	}
	twDataShape_AddEntry(ds,twDataShapeEntry_Create("value", NULL, TW_VARIANT));
	twDataShape_AddEntry(ds,twDataShapeEntry_Create("time", NULL, TW_DATETIME));
	twDataShape_AddEntry(ds,twDataShapeEntry_Create("quality", NULL, TW_STRING));
	/* Create the infotable */
	it = twInfoTable_Create(ds);
	if (!it) {
		TW_LOG(TW_ERROR,"twApi_PushProperties: Error creating infotable");
		twDataShape_Delete(ds);
		return TWX_INTERNAL_SERVER_ERROR;
	}
	/* Loop through the list and create a row per entry */
	le = twList_Next(properties, NULL);
	while (le) {
		twProperty * prop = (twProperty *)le->value;
		twInfoTableRow * row = NULL;
		row = twInfoTableRow_Create(twPrimitive_CreateFromString(prop->name, TRUE));
		if (!row) {
			TW_LOG(TW_ERROR,"twApi_PushProperties: Error creating infotable row");
			break;
		}
		twInfoTableRow_AddEntry(row, twPrimitive_CreateVariant(twPrimitive_ZeroCopy(prop->value)));
		twInfoTableRow_AddEntry(row,twPrimitive_CreateFromDatetime(prop->timestamp));
		twInfoTableRow_AddEntry(row,twPrimitive_CreateFromString("GOOD",TRUE));
		twInfoTable_AddRow(it,row);
		le = twList_Next(properties, le);
	}
	/* Make the service request */
	values = twInfoTable_Create(twDataShape_Create(twDataShapeEntry_Create("values", NULL, TW_INFOTABLE)));
	twInfoTable_AddRow(values, twInfoTableRow_Create(twPrimitive_CreateFromInfoTable(it)));
	res = twApi_InvokeService(entityType, entityName, "UpdateSubscribedPropertyValues", values, &result, timeout, forceConnect);
	twInfoTable_Delete(it);
	twInfoTable_Delete(values);
	twInfoTable_Delete(result);
	return res;
}

int twApi_InvokeService(enum entityTypeEnum entityType, char * entityName, char * serviceName, twInfoTable * params, twInfoTable ** result, int32_t timeout, char forceConnect) {
	enum msgCodeEnum res = makeRequest(TWX_POST, entityType, entityName, TW_SERVICES, serviceName, params, result, timeout, forceConnect);
	return convertMsgCodeToErrorCode(res);
}

int twApi_FireEvent(enum entityTypeEnum entityType, char * entityName, char * eventName, twInfoTable * params, int32_t timeout, char forceConnect) {
	twInfoTable * result = NULL;
	enum msgCodeEnum res;
	res = makeRequest(TWX_POST, entityType, entityName, TW_EVENTS, eventName, params, &result, timeout, forceConnect);
	twInfoTable_Delete(result);
	return convertMsgCodeToErrorCode(res);
}

int twApi_RegisterConnectCallback(eventcb cb) {
	if (tw_api && tw_api->mh) return twMessageHandler_RegisterConnectCallback(tw_api->mh, cb);
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

int twApi_RegisterCloseCallback(eventcb cb) {
	if (tw_api && tw_api->mh) return twMessageHandler_RegisterCloseCallback(tw_api->mh, cb);
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

int twApi_RegisterPingCallback(eventcb cb) {
	if (tw_api && tw_api->mh) return twMessageHandler_RegisterPingCallback(tw_api->mh, cb);
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

int twApi_RegisterPongCallback(eventcb cb) {
	if (tw_api && tw_api->mh){
		/* We are no longer handling pongs in the api */
		tw_api->handle_pongs = FALSE;
		return twMessageHandler_RegisterPongCallback(tw_api->mh, cb);
	}
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

int twApi_RegisterBindEventCallback(char * entityName, bindEvent_cb cb, void * userdata) {
	callbackInfo * info = (callbackInfo *)TW_CALLOC(sizeof(callbackInfo), 1);
	if (!tw_api || !tw_api->bindEventCallbackList || !cb) return TW_NULL_OR_INVALID_API_SINGLETON;
	if (!info) return TW_ERROR_ALLOCATING_MEMORY;
	if (entityName) info->entityName = duplicateString(entityName);
	info->cb = cb;
	info->userdata = userdata;
	return twList_Add(tw_api->bindEventCallbackList, info);
}

int twApi_UnregisterBindEventCallback(char * entityName, bindEvent_cb cb, void * userdata) {
	ListEntry * le = NULL;
	if (!tw_api || !tw_api->bindEventCallbackList) return TW_NULL_OR_INVALID_API_SINGLETON;
	le = twList_Next(tw_api->bindEventCallbackList, NULL);
	while (le && le->value) {
		callbackInfo * info = (callbackInfo *)le->value;
		if (info->cb == cb && info->userdata == userdata && !strcmp(info->entityName, entityName)) {
			twList_Remove(tw_api->bindEventCallbackList, le, TRUE);
			return TW_OK;
		}
		le = twList_Next(tw_api->bindEventCallbackList, le);
	}
	return TW_NOT_FOUND;
}

int twApi_RegisterOnAuthenticatedCallback(authEvent_cb cb, void * userdata) {
	callbackInfo * info = (callbackInfo *)TW_CALLOC(sizeof(callbackInfo), 1);
	if (!tw_api || !tw_api->bindEventCallbackList || !cb) return TW_NULL_OR_INVALID_API_SINGLETON;
	if (!info) return TW_ERROR_ALLOCATING_MEMORY;
	/* Just using TW_APPLICATIONKEYS as a non-zero flag - no special significance */
	info->entityType = TW_APPLICATIONKEYS;
	info->cb = cb;
	info->userdata = userdata;
	return twList_Add(tw_api->bindEventCallbackList, info);
}

int twApi_UnregisterOnAuthenticatedCallback(authEvent_cb cb, void * userdata) {
	ListEntry * le = NULL;
	if (!tw_api || !tw_api->bindEventCallbackList) return TW_NULL_OR_INVALID_API_SINGLETON;
	le = twList_Next(tw_api->bindEventCallbackList, NULL);
	while (le && le->value) {
		callbackInfo * info = (callbackInfo *)le->value;
		if (info->cb == cb && info->userdata == userdata && info->entityType == TW_APPLICATIONKEYS) {
			twList_Remove(tw_api->bindEventCallbackList, le, TRUE);
			return TW_OK;
		}
		le = twList_Next(tw_api->bindEventCallbackList, le);
	}
	return TW_NOT_FOUND;
}

int twApi_CleanupOldMessages() {
	if (tw_api && tw_api->mh) return twMessageHandler_CleanupOldMessages(tw_api->mh);
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

int twApi_SendPing(char * content) {
	if (tw_api && tw_api->mh && tw_api->mh->ws) return twWs_SendPing(tw_api->mh->ws, content);
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

int twApi_CreateTask(uint32_t runTintervalMsec, twTaskFunction func) {
	return twTasker_CreateTask(runTintervalMsec, func);
}

void twApi_SetSelfSignedOk() {
	if (tw_api && tw_api->mh && tw_api->mh->ws) twTlsClient_SetSelfSignedOk(tw_api->mh->ws->connection);
	if (tw_api->connectionInfo) tw_api->connectionInfo->selfsignedOk = TRUE;
}

void twApi_DisableCertValidation() {
	if (tw_api && tw_api->mh && tw_api->mh->ws) twTlsClient_DisableCertValidation(tw_api->mh->ws->connection);
	if (tw_api->connectionInfo) tw_api->connectionInfo->doNotValidateCert = TRUE;
}

int	twApi_LoadCACert(const char *file, int type) {
	if (tw_api && tw_api->mh && tw_api->mh->ws) {
		if (tw_api->connectionInfo) {
			if (tw_api->connectionInfo->ca_cert_file) {
				TW_FREE(tw_api->connectionInfo->ca_cert_file);
				tw_api->connectionInfo->ca_cert_file = NULL;
			}
			tw_api->connectionInfo->ca_cert_file = duplicateString(file);
		}
		return twTlsClient_UseCertificateChainFile(tw_api->mh->ws->connection, file,type);
	}
	else return TW_NULL_OR_INVALID_MSG_HANDLER;
}

int	twApi_LoadClientCert(char *file) {
	if (tw_api && tw_api->mh && tw_api->mh->ws) {
		if (tw_api->connectionInfo) {
			if (tw_api->connectionInfo->client_cert_file) {
				TW_FREE(tw_api->connectionInfo->client_cert_file);
				tw_api->connectionInfo->client_cert_file = NULL;
			}
			tw_api->connectionInfo->client_cert_file = duplicateString(file);
		}
		return twTlsClient_SetClientCaList(tw_api->mh->ws->connection, file);
	}
	else return TW_NULL_OR_INVALID_MSG_HANDLER;
}

int	twApi_SetClientKey(const char *file, char * passphrase, int type) {
	if (tw_api && tw_api->mh && tw_api->mh->ws) {
		if (!passphrase || !file) return TW_INVALID_PARAM;
		if (tw_api->connectionInfo) {
			if (tw_api->connectionInfo->client_key_file) {
				TW_FREE(tw_api->connectionInfo->client_key_file);
				tw_api->connectionInfo->client_key_file = NULL;
			}
			if (tw_api->connectionInfo->client_key_passphrase) {
				TW_FREE(tw_api->connectionInfo->client_key_passphrase);
				tw_api->connectionInfo->client_key_passphrase = NULL;
			}
			tw_api->connectionInfo->client_key_file = duplicateString(file);
			tw_api->connectionInfo->client_key_passphrase = duplicateString(passphrase);
		}
		twTlsClient_SetDefaultPasswdCbUserdata(tw_api->mh->ws->connection, passphrase);
		return twTlsClient_UsePrivateKeyFile(tw_api->mh->ws->connection, file, type);
	} else return TW_NULL_OR_INVALID_MSG_HANDLER;
}

int twApi_EnableFipsMode()  {
	if (tw_api && tw_api->mh && tw_api->mh->ws) {
		if (tw_api->connectionInfo) tw_api->connectionInfo->fips_mode = TRUE;
		return twTlsClient_EnableFipsMode(tw_api->mh->ws->connection);
	}
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

void twApi_DisableEncryption() {
	if (tw_api && tw_api->mh && tw_api->mh->ws) {
		if (tw_api->connectionInfo) tw_api->connectionInfo->disableEncryption = TRUE;
		twTlsClient_DisableEncryption(tw_api->mh->ws->connection);
	}
	return;
}

int twApi_SetX509Fields(char * subject_cn, char * subject_o, char * subject_ou,
							  char * issuer_cn, char * issuer_o, char * issuer_ou) {
	if (tw_api && tw_api->mh && tw_api->mh->ws) {
		if (tw_api->connectionInfo) {
			if (tw_api->connectionInfo->subject_cn) {
				TW_FREE(tw_api->connectionInfo->subject_cn);
				tw_api->connectionInfo->subject_cn = NULL;
			}
			tw_api->connectionInfo->subject_cn = duplicateString(subject_cn);
			if (tw_api->connectionInfo->subject_o) {
				TW_FREE(tw_api->connectionInfo->subject_o);
				tw_api->connectionInfo->subject_o = NULL;
			}
			tw_api->connectionInfo->subject_o = duplicateString(subject_o);
			if (tw_api->connectionInfo->subject_ou) {
				TW_FREE(tw_api->connectionInfo->subject_ou);
				tw_api->connectionInfo->subject_ou = NULL;
			}
			tw_api->connectionInfo->subject_ou = duplicateString(subject_ou);
			if (tw_api->connectionInfo->issuer_cn) {
				TW_FREE(tw_api->connectionInfo->issuer_cn);
				tw_api->connectionInfo->issuer_cn = NULL;
			}
			tw_api->connectionInfo->issuer_cn = duplicateString(issuer_cn);
			if (tw_api->connectionInfo->issuer_o) {
				TW_FREE(tw_api->connectionInfo->issuer_o);
				tw_api->connectionInfo->issuer_o = NULL;
			}
			tw_api->connectionInfo->issuer_o = duplicateString(issuer_o);
			if (tw_api->connectionInfo->issuer_ou) {
				TW_FREE(tw_api->connectionInfo->issuer_ou);
				tw_api->connectionInfo->issuer_ou = NULL;
			}
			tw_api->connectionInfo->issuer_ou = duplicateString(issuer_ou);
		}
		return twTlsClient_SetX509Fields(tw_api->mh->ws->connection, subject_cn, subject_o, subject_ou, issuer_cn, issuer_o, issuer_ou);
	}
	return TW_NULL_OR_INVALID_API_SINGLETON;
}

twConnectionInfo * twApi_GetConnectionInfo() {
	return tw_api->connectionInfo;
}

int	twApi_SetOfflineMsgStoreDir(const char *dir) {
#if (OFFLINE_MSG_STORE == 2)
	if (!tw_api) {
		TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: tw_api is not initialized");
		return TW_NULL_API_SINGLETON;
	}
	if (!dir) {
		TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: NULL directory string");
		return TW_INVALID_PARAM;
	}
	/* Make sure any existing file is not empty */
	if (tw_api->offlineMsgFile || tw_api->subscribedPropsFile) {
		uint64_t size = 0;
		DATETIME lastModified;
		char isDir;
		char isReadOnly;
		if (tw_api->offlineMsgFile) {
			/* Are we changing the directory at all?  If not just return success */
			char * tmp = strstr(tw_api->offlineMsgFile, dir);
			if (tmp == tw_api->offlineMsgFile) {
				/* File start with 'dir', now check to see if there are any extra subdirs*/
				tmp += strlen(dir);
				if (!strcmp(tmp,"/offline_msgs.bin")) {
					/* The new dir is the same as the old - nothing to do except set the sizes */
					uint64_t size = 0;
					DATETIME lastModified;
					char isDir;
					char isReadOnly;
					/* Get the size of the file in case there are some persisted messages */
					twDirectory_GetFileInfo(tw_api->offlineMsgFile, &size, &lastModified, &isDir, &isReadOnly);
					tw_api->offlineMsgSize = size;
					size = 0;
					twDirectory_GetFileInfo(tw_api->subscribedPropsFile, &size, &lastModified, &isDir, &isReadOnly);
					tw_api->subscribedPropsSize = size;
					TW_LOG(TW_DEBUG, "twApi_SetOfflineMsgStoreDir: New dir %s is same as the original", dir);
					return TW_OK;
				}
			}
			/* Get the size of the file in case there are some persisted messages.  Error out if its non-zero in length */
			twDirectory_GetFileInfo(tw_api->offlineMsgFile, &size, &lastModified, &isDir, &isReadOnly);
			if (size > 0) {
				TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Existing offline messge store file %s is not empty", tw_api->offlineMsgFile);
				return TW_MSG_STORE_FILE_NOT_EMPTY;
			}
			/* Get rid of the existing file */
			twDirectory_DeleteFile(tw_api->offlineMsgFile);
			TW_FREE(tw_api->offlineMsgFile);
			tw_api->offlineMsgFile = NULL;
		}
		if (tw_api->subscribedPropsFile) {
			size = 0;
			/* Get the size of the file in case there are some persisted messages.  Error out if its non-zero in length */
			twDirectory_GetFileInfo(tw_api->subscribedPropsFile, &size, &lastModified, &isDir, &isReadOnly);
			if (size > 0) {
				TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Existing subscribed properties file %s is not empty", tw_api->subscribedPropsFile);
				return TW_MSG_STORE_FILE_NOT_EMPTY;
			}
			/* Get rid of the existing file */
			twDirectory_DeleteFile(tw_api->subscribedPropsFile);
			TW_FREE(tw_api->subscribedPropsFile);
			tw_api->subscribedPropsFile = NULL;
		}
	} 
	/* Create the directory and the files if needed */
	if (twDirectory_CreateDirectory((char *)dir)) {
		TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Error creating offline message directory %s",dir);
		return TW_INVALID_MSG_STORE_DIR;
	} else {
		/* Persisted offline messages */
		tw_api->offlineMsgFile = (char *)TW_CALLOC(strlen(dir) + strlen("offline_msgs.bin") + 2, 1);
		if (tw_api->offlineMsgFile) {
			uint64_t size = 0;
			DATETIME lastModified;
			char isDir;
			char isReadOnly;
			strcpy(tw_api->offlineMsgFile, dir);
			strcat(tw_api->offlineMsgFile,"/");
			strcat(tw_api->offlineMsgFile,"offline_msgs.bin");
			/* If the file doesn't exist, create it */
			if (!twDirectory_FileExists(tw_api->offlineMsgFile)) {
				if (twDirectory_CreateFile(tw_api->offlineMsgFile)) {
					TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Error creating offline message file %s", tw_api->offlineMsgFile);
					TW_FREE(tw_api->offlineMsgFile);
					tw_api->offlineMsgFile = NULL;
				} else {
					TW_LOG(TW_INFO, "twApi_SetOfflineMsgStoreDir: Created offline message file %s", tw_api->offlineMsgFile);
				}
			} else {
				/* Get the size of the file in case there are some persisted messages */
				twDirectory_GetFileInfo(tw_api->offlineMsgFile, &size, &lastModified, &isDir, &isReadOnly);
			}
			tw_api->offlineMsgSize = size;
		} else {
			TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Error allocating offline message file name");
			return TW_ERROR_ALLOCATING_MEMORY;
		}
		/* Now do the persited subscribed props */
		tw_api->subscribedPropsFile = (char *)TW_CALLOC(strlen(dir) + strlen("subscribed_props.bin") + 2, 1);
		if (tw_api->subscribedPropsFile) {
			uint64_t size = 0;
			DATETIME lastModified;
			char isDir;
			char isReadOnly;
			strcpy(tw_api->subscribedPropsFile, dir);
			strcat(tw_api->subscribedPropsFile,"/");
			strcat(tw_api->subscribedPropsFile,"subscribed_props.bin");
			/* If the file doesn't exist, create it */
			if (!twDirectory_FileExists(tw_api->subscribedPropsFile)) {
				if (twDirectory_CreateFile(tw_api->subscribedPropsFile)) {
					TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Error creating subscribed props file %s", tw_api->subscribedPropsFile);
					TW_FREE(tw_api->subscribedPropsFile);
					tw_api->subscribedPropsFile = NULL;
				} else {
					TW_LOG(TW_INFO, "twApi_SetOfflineMsgStoreDir: Created subscribed file %s", tw_api->subscribedPropsFile);
				}
			} else {
				/* Get the size of the file in case there are some persisted messages */
				twDirectory_GetFileInfo(tw_api->subscribedPropsFile, &size, &lastModified, &isDir, &isReadOnly);
			}
			tw_api->subscribedPropsSize = size;
		} else {
			TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Error allocatingsubscribed property file name");
			return TW_ERROR_ALLOCATING_MEMORY;
		}
	}
	return TW_OK;
#else
	TW_LOG(TW_ERROR, "twApi_SetOfflineMsgStoreDir: Persisted offline message store not enabled");
	return TW_ERROR;
#endif
}

/* Structure to allow overriding of defaults at runtime */
twConfig twcfg =  { 
#ifdef ENABLE_TASKER
	TRUE, 
#else
	FALSE,
#endif	
#ifdef ENABLE_FILE_XFER
	TRUE, 
#else
	FALSE,
#endif	
#ifdef ENABLE_TUNNELING
	TRUE, 
#else
	FALSE,
#endif	
#ifdef OFFLINE_MSG_STORE
	OFFLINE_MSG_STORE,
#else
	2,
#endif
	TW_URI, 
	MAX_MESSAGE_SIZE, 
	MESSAGE_CHUNK_SIZE, 
	DEFAULT_MESSAGE_TIMEOUT, 
	PING_RATE, 
	DEFAULT_PONG_TIMEOUT,
	STALE_MSG_CLEANUP_RATE, 
	CONNECT_TIMEOUT, 
	CONNECT_RETRIES, 
	DUTY_CYCLE, 
	DUTY_CYCLE_PERIOD, 
	STREAM_BLOCK_SIZE,
	FILE_XFER_BLOCK_SIZE, 
	FILE_XFER_MAX_FILE_SIZE,	
	FILE_XFER_MD5_BLOCK_SIZE, 
	FILE_XFER_TIMEOUT, 
	FILE_XFER_STAGING_DIR, 
	OFFLINE_MSG_QUEUE_SIZE, 
	MAX_CONNECT_DELAY, 
	CONNECT_RETRY_INTERVAL, 
	MAX_MESSAGES, 
	DEFAULT_SOCKET_READ_TIMEOUT,
	DEFAULT_SSL_READ_TIMEOUT,
	OFFLINE_MSG_STORE_DIR,
	FRAME_READ_TIMEOUT
}; 

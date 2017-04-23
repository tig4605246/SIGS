/*
 *  Copyright (C) 2015 ThingWorx Inc.
 *
 *  Thingworx Subscribed Properties
 */

#include "twSubscribedProps.h"
#include "twApi.h"
#include "stringUtils.h"
#include "twList.h"
#include "twInfoTable.h"
#include "twProperties.h"

#include <limits.h>

/* Reference to tw_api */
extern twApi * tw_api;

/***************************************/
/*     Subscribed Property struct      */
/***************************************/
#define TW_UPDATE_ALWAYS 0
#define TW_UPDATE_NEVER 1
#define TW_UPDATE_VALUE 2
#define TW_UPDATE_ON 3
#define TW_UPDATE_OFF 4

typedef struct twSubscribedProperty {
	const char * entity;
	twProperty * prop;
	char fold;
} twSubscribedProperty;

twSubscribedProperty * twSubscribedProperty_Create (char * e, char * n, twPrimitive * v, DATETIME t, char * q, char fold) ;
twSubscribedProperty * twSubscribedProperty_CreateFromStream(twStream * s);
void twSubscribedProperty_Delete(void * prop);
int twSubscribedProperty_ToStream(twSubscribedProperty * p, twStream * s);
uint32_t  twSubscribedProperty_GetLength(twSubscribedProperty * p);

/***************************************/
/*     Entity Saved Property List     */
/***************************************/
typedef struct entitySavedProperties {
	char * name;
	twList * props;
} entitySavedProperties;

void entitySavedProperties_Delete(void * entity) {
	entitySavedProperties * e = (entitySavedProperties *)entity;
	if (!e) return;
	if (e->name) TW_FREE(e->name);
	if (e->props) twList_Delete(e->props);
	TW_FREE(e);
}

entitySavedProperties * entitySavedProperties_Create(char * name) {
	entitySavedProperties * e = NULL;
	if (!name) return NULL;
	e = (entitySavedProperties *)TW_CALLOC(sizeof(entitySavedProperties), 1);
	if (!e) return NULL;
	e->name = duplicateString(name);
	e->props = twList_Create(twSubscribedProperty_Delete);
	if (!e->name || !e->props) {
		entitySavedProperties_Delete(e);
		return NULL;
	}
	return e;
}

/***************************************/
/*    Subscribed Properties Manager    */
/***************************************/
typedef struct twSubscribedPropsMgr {
	TW_MUTEX mtx;
	twList * currentValues;
	twList * savedValues;
	twStream * persistedValues;
	char fold;
	twInfoTable * itTemplate;
	uint32_t queueSize;
} twSubscribedPropsMgr;

/* Our singleton */
twSubscribedPropsMgr * spm = NULL;

#define MSG_OVERHEAD 100
int twSubscribedPropsMgr_PushPropertyList(twList * list, char forceConnect) {
	twInfoTable * it = NULL;
    twInfoTable * values = NULL;
	ListEntry * entityListEntry = NULL;
	twInfoTable * resultIt = NULL;
	int res = TW_OK;
	if (!spm || !spm->itTemplate || !list) {
		TW_LOG(TW_ERROR, "twSubscribedPropsMgr_PushPropertyList: spm not initialized");
		return TW_SUBSCRIBEDPROP_MGR_NOT_INTIALIZED;
	}
	/* Work thorugh all the lists for each entity */
	entityListEntry = twList_Next(list, NULL);
	while (entityListEntry && entityListEntry->value) {
		ListEntry * le = NULL;
		entitySavedProperties * e = (entitySavedProperties *)entityListEntry->value;
		char sentAll = FALSE;
		int32_t index = 0;
		/* We need to watch the size so we don't exceed the max message size */
		while (!sentAll) {
			/* Create our infotable */
			it = twInfoTable_FullCopy(spm->itTemplate);
			if (!it) {
				TW_LOG(TW_INFO, "twSubscribedPropsMgr_PushPropertyList: Error creating infotable.");
				return TW_ERROR_ALLOCATING_MEMORY;
			}
			/* Loop through the  entity list and create a row per entry */
			if (index < twList_GetCount(e->props)) le = twList_GetByIndex(e->props, index);
			while (le && le->value && it->length + MSG_OVERHEAD < twcfg.max_message_size) {
				twSubscribedProperty * p = (twSubscribedProperty *)le->value;
				twInfoTableRow * row = NULL;
				row = twInfoTableRow_Create(twPrimitive_CreateFromString(p->prop->name, TRUE));
				if (!row) {
					TW_LOG(TW_ERROR,"twSubscribedPropsMgr_PushPropertyList: Error creating infotable row");
					break;
				}
				if (twInfoTableRow_GetLength(row) + it->length + MSG_OVERHEAD > twcfg.max_message_size) {
					twInfoTableRow_Delete(row);
					index--;
					break;
				}
				twInfoTableRow_AddEntry(row, twPrimitive_CreateVariant(twPrimitive_FullCopy(p->prop->value)));
				twInfoTableRow_AddEntry(row,twPrimitive_CreateFromDatetime(p->prop->timestamp));
				twInfoTableRow_AddEntry(row,twPrimitive_CreateFromString(p->prop->quality,TRUE));
				twInfoTable_AddRow(it,row);
				index++;
				le = twList_Next(e->props, le);
			}
			/* Was this the last one? */
			if (le == NULL) {
				sentAll = TRUE;
			} 
			/* Make the service request */
			if (!twApi_isConnected() && !forceConnect) {
	#if (OFFLINE_MSG_STORE == 2) 
				if (tw_api->offlineMsgFile) {
					ListEntry * persistRow = NULL;
					char persistError = FALSE;
					/* Write all the rows plus the subscribed property extra data (entityName & fold) to our persisted file */
					TW_LOG(TW_TRACE, "twSubscribedPropsMgr_PushPropertyList: Currently not connected. Writing properties for %s to persisted storage", e->name);
					if (spm->persistedValues && list == spm->savedValues) {
						persistRow = twList_Next(e->props, NULL);
						while (persistRow && persistRow->value) {
							if (twStream_GetLength(spm->persistedValues) < twcfg.offline_msg_queue_size) {
								twSubscribedProperty_ToStream((twSubscribedProperty *)persistRow->value, spm->persistedValues);
							} else {
								TW_LOG(TW_WARN, "twSubscribedPropsMgr_PushPropertyList: Writing persisted property would exceed maximum storage");
								persistError = TRUE;
								break;
							}
							persistRow = twList_Next(e->props, persistRow);
						}
					} else if (list == spm->savedValues) {
						TW_LOG(TW_ERROR, "twSubscribedPropsMgr_PushPropertyList: Persisted property storage is not initialized");
						persistError = TRUE;
					}
					/* 
					If we aren't connected but persisted message store is enabled we have written all values to disk
					and should remove them from RAM so we don't double send them when we are reconnected
					*/
					if (!persistError) {
						/* Remove this from the list */
						ListEntry * tmp = entityListEntry->prev;
						twList_Remove(list, entityListEntry, TRUE);
						entityListEntry = tmp;
					}
					twInfoTable_Delete(it);
					it = NULL;
				}  else {
					TW_LOG(TW_WARN, "subscribedPropsMgr_Initialize: property list will not persist offline pushes because the offline message directory was not set or the offline message store was disabled");
				}
	#endif
				TW_LOG(TW_INFO, "twSubscribedPropsMgr_PushPropertyList: Not connected. Send will not be attempted.");
			} else {
				TW_LOG(TW_TRACE, "twSubscribedPropsMgr_PushPropertyList: Pushing all properties for %s.", e->name);
                values = twInfoTable_Create(twDataShape_Create(twDataShapeEntry_Create("values", NULL, TW_INFOTABLE)));
                twInfoTable_AddRow(values, twInfoTableRow_Create(twPrimitive_CreateFromInfoTable(it)));
				res = twApi_InvokeService(TW_THING, e->name, "UpdateSubscribedPropertyValues", values, &resultIt, -1, forceConnect);
				twInfoTable_Delete(it);
				it = NULL;
				twInfoTable_Delete(resultIt);	
				if (res && res != TW_WROTE_TO_OFFLINE_MSG_STORE) {
					TW_LOG(TW_ERROR, "twSubscribedPropsMgr_PushPropertyList: Error pushing properties for %s.  Error: %d.", e->name, res);
				} else {
					if (sentAll) {
						/* We have successfully sent the values for this entity, so Remove this from the outer list */
						ListEntry * tmp = entityListEntry->prev;
						twList_Remove(list, entityListEntry, TRUE);
						entityListEntry = tmp;
                        twInfoTable_Delete(values);
                        values = NULL;
					} else {
						continue;
					}
				}
			}
		}
		entityListEntry = twList_Next(list, entityListEntry);
	}
	return res;
}

int twSubscribedPropsMgr_QueueValueForSending(twSubscribedProperty * p, twList * list) {
	ListEntry * propListEntry = NULL;
	ListEntry * entityListEntry = NULL;
	char foundEntity = FALSE;
	char foundProperty = FALSE;
	char * src = "RAM";
	if (!p || !p->entity || !p->prop || !p->prop->name || !p->prop->value || !p->prop->quality || !list) {
		TW_LOG(TW_ERROR, "twSubscribedPropsMgr_QueueValueForSending: NULL pointer found");
		return TW_INVALID_PARAM;
	}
	TW_LOG(TW_TRACE, "twSubscribedPropsMgr_QueueValueForSending: Updating saved property value. Property: %s. Folding is %s", p->prop->name, p->fold ? "ON" : "OFF");
	if (list != spm->savedValues) src = "FILE";
	/* Each entity has its own list of properties - so the list parameter is actually is a list of lists */
	/* See if this entity already has a property list.  If not, create it */
	entityListEntry = twList_Next(list, NULL);
	while (entityListEntry && entityListEntry->value) {
		entitySavedProperties * e = (entitySavedProperties *)entityListEntry->value;
		if (!strcmp(e->name, p->entity)) {
			/* Found a match for the entity*/ 
			foundEntity = TRUE;
			if (!p->fold) {
				/*if we are not folding just add the property */
				TW_LOG(TW_TRACE, "twSubscribedPropsMgr_QueueValueForSending: Adding property %s to entity %s list. Source: %s", p->prop->name, p->entity, src);
				twList_Add(e->props, p);
				break;
			} else {
				/* We are folding so we need to find and replace the property, or add it if it doesn't exist */
				propListEntry = twList_Next(e->props, NULL);
				while (propListEntry && propListEntry->value) {
					twSubscribedProperty * s = (twSubscribedProperty *)propListEntry->value;
					if (s->prop && s->prop->name && !strcmp(s->prop->name, p->prop->name)) {
						TW_LOG(TW_TRACE, "twSubscribedPropsMgr_QueueValueForSending: Folding property %s into entity %s list.  Source: %s", p->prop->name, p->entity, src);
						twList_ReplaceValue(e->props, propListEntry, p, TRUE);
						foundProperty = TRUE;
						break;
					}
					propListEntry = twList_Next(e->props, propListEntry);
				}
				if (!foundProperty) {
					TW_LOG(TW_TRACE, "twSubscribedPropsMgr_QueueValueForSending: Adding first instance of property %s to entity %s list", p->prop->name,  p->entity);
					twList_Add(e->props, p);
				}
			}
			entityListEntry = twList_Next(list, entityListEntry);
		}
	}
	if (!foundEntity) {
		/* Need to create a list for this entity */
		entitySavedProperties * esp = entitySavedProperties_Create((char *)p->entity);
		TW_LOG(TW_TRACE, "twSubscribedPropsMgr_QueueValueForSending: Creating property list for entity %s", p->entity);
		if (!esp) {
			TW_LOG(TW_ERROR, "twSubscribedPropsMgr_QueueValueForSending: Error creating property list for %s", p->entity);
			return TW_ERROR_ALLOCATING_MEMORY;
		}
		twList_Add(list,esp);
		TW_LOG(TW_TRACE, "twSubscribedPropsMgr_QueueValueForSending: Adding property %s to entity %s list", p->entity, p->prop->name);
		twList_Add(esp->props, p);
	}
	return TW_OK;
}

int twSubscribedPropsMgr_SendPersistedValues() {
#if (OFFLINE_MSG_STORE == 2) 
	if (tw_api->offlineMsgFile) {
		twSubscribedProperty * p = NULL;
		twList * valuesToPush = NULL;
		TW_LOG(TW_TRACE, "subscribedPropsMgr_RetrievePersistedValues: Retrieving persisted values from file %s", tw_api->subscribedPropsFile ? tw_api->subscribedPropsFile : "UNKNOWN");
		if (!spm || !spm->persistedValues) {
			TW_LOG(TW_ERROR, "subscribedPropsMgr_UpdateSavedValues: spm->persisteValues not initialized");
			return TW_SUBSCRIBEDPROP_MGR_NOT_INTIALIZED;
		}
		/* If the file stream is empty there is nothing to do */
		if (twStream_GetLength(spm->persistedValues) == 0) {
			TW_LOG(TW_TRACE, "subscribedPropsMgr_UpdateSavedValues: %s is empty, nothing to do", tw_api->subscribedPropsFile ? tw_api->subscribedPropsFile : "UNKNOWN");
			return TW_OK;
		}
		/* Need to do any property folding here since it is not done when persisting */
		valuesToPush = twList_Create(entitySavedProperties_Delete);
		if (!valuesToPush) {
			TW_LOG(TW_ERROR, "subscribedPropsMgr_UpdateSavedValues: Error allocating memory for temporary list");
			return TW_ERROR_ALLOCATING_MEMORY;
		}
		/* Persisted properties are stored as a contiguouos list of serialized twSubscribedProperties */
		p = twSubscribedProperty_CreateFromStream(spm->persistedValues);
		while (p) {
			if (twSubscribedPropsMgr_QueueValueForSending(p, valuesToPush)) {
				TW_LOG(TW_ERROR, "subscribedPropsMgr_UpdateSavedValues: Error queueing persisted property %s:%s", p->entity, p->prop->name);
			}
			p = twSubscribedProperty_CreateFromStream(spm->persistedValues);
		}
		/* Push the properties to the server */
		if (twSubscribedPropsMgr_PushPropertyList(valuesToPush, FALSE)) {
			TW_LOG(TW_ERROR, "subscribedPropsMgr_UpdateSavedValues: Error pushing properties to the server");
			twList_Delete(valuesToPush);
			/* Reset to the beginning of the stream for next time */
			twStream_Reset(spm->persistedValues);
			return TW_ERROR;
		} else {
			/* If we are here we have successfully pushed all persisted properties, reopen the file to clear it */
			TW_FILE_HANDLE f = NULL;
			twStream_Delete(spm->persistedValues);
			spm->persistedValues = NULL;
			f = TW_FOPEN(tw_api->subscribedPropsFile, "w");
			TW_FCLOSE(f);
			spm->persistedValues = twStream_CreateFromFile(tw_api->subscribedPropsFile);
			twList_Delete(valuesToPush);
		}
	} else {
		TW_LOG(TW_WARN, "subscribedPropsMgr_Initialize: subscribed property values will not persist to disk while offline because the offline message directory was not set or the offline message store was disabled");
	}
#endif
	return TW_OK;
}

int twSubscribedPropsMgr_Initialize() {
	twSubscribedPropsMgr * tmp = NULL;
	twDataShape * ds = NULL;
	TW_LOG(TW_DEBUG, "subscribedPropsMgr_Initialize: Initializing subscribed properties manager");
	tmp = (twSubscribedPropsMgr *)TW_CALLOC(sizeof(twSubscribedPropsMgr), 1);
	if (!tmp) {
		TW_LOG(TW_ERROR, "subscribedPropsMgr_Initialize: Error allocating memory");
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	tmp->mtx = twMutex_Create();
	tmp->currentValues = twList_Create(twSubscribedProperty_Delete);
	tmp->savedValues = twList_Create(entitySavedProperties_Delete);
	/* Create our infotable template */
	/* Create the data shape */
	ds = twDataShape_Create(twDataShapeEntry_Create("name", NULL, TW_STRING));
	if (!ds) {
		if(tmp->mtx) twMutex_Delete(tmp->mtx);
		if (tmp->currentValues) twList_Delete(tmp->currentValues);
		if (tmp->savedValues) twList_Delete(tmp->savedValues);
		TW_LOG(TW_ERROR,"subscribedPropsMgr_Initialize: Error allocating data shape");
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	twDataShape_AddEntry(ds,twDataShapeEntry_Create("value", NULL, TW_VARIANT));
	twDataShape_AddEntry(ds,twDataShapeEntry_Create("time", NULL, TW_DATETIME));
	twDataShape_AddEntry(ds,twDataShapeEntry_Create("quality", NULL, TW_STRING));
	tmp->itTemplate = twInfoTable_Create(ds);

#if (OFFLINE_MSG_STORE == 2) 
	if (tw_api->offlineMsgFile) {
		tmp->persistedValues = twStream_CreateFromFile(tw_api->subscribedPropsFile);
		if (!tmp->persistedValues) {
			if (tmp->mtx) twMutex_Delete(tmp->mtx);
			if (tmp->currentValues) twList_Delete(tmp->currentValues);
			if (tmp->savedValues) twList_Delete(tmp->savedValues);
			if (tmp->persistedValues) twStream_Delete(tmp->persistedValues);
			if (tmp->itTemplate) twInfoTable_Delete(tmp->itTemplate);
			TW_LOG(TW_ERROR, "subscribedPropsMgr_Initialize: Error opening: %s", tw_api->subscribedPropsFile ? tw_api->subscribedPropsFile : "no filename found");
			return TW_ERROR_ALLOCATING_MEMORY;
		}
	} else {
		TW_LOG(TW_WARN, "subscribedPropsMgr_Initialize: subscribed property manager will not persist offline updates because the offline message directory was not set or the offline message store was disabled");
	}
#endif
	if (!tmp->mtx || !tmp->currentValues || !tmp->savedValues || !tmp->itTemplate) {
		if (tmp->mtx) twMutex_Delete(tmp->mtx);
		if (tmp->currentValues) twList_Delete(tmp->currentValues);
		if (tmp->savedValues) twList_Delete(tmp->savedValues);
		if (tmp->persistedValues) twStream_Delete(tmp->persistedValues);
		if (tmp->itTemplate) twInfoTable_Delete(tmp->itTemplate);
		TW_LOG(TW_ERROR, "subscribedPropsMgr_Initialize: Error allocating structure member");
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	spm = tmp;

	return TW_OK;
}

void twSubscribedPropsMgr_SetFolding(char fold) {
	if (spm) spm->fold = fold;
}

void twSubscribedPropsMgr_Delete() {
	TW_LOG(TW_INFO, "subscribedPropsMgr_Delete: Deleting subscribed propery manager");
	if (!spm) return;
	if (spm->mtx) twMutex_Lock(spm->mtx);
	if (spm->currentValues) twList_Delete(spm->currentValues);
	if (spm->savedValues) twList_Delete(spm->savedValues);
	if (spm->persistedValues) twStream_Delete(spm->persistedValues);
	if (spm->itTemplate) twInfoTable_Delete(spm->itTemplate);
	if (spm->mtx) {
		twMutex_Unlock(spm->mtx);
		twMutex_Delete(spm->mtx);
	}
	TW_FREE(spm);
}

int twSubscribedPropsMgr_PushSubscribedProperties(char * entityName, char forceConnect) {
	if (!spm || !spm->mtx || !spm->savedValues) {
		TW_LOG(TW_ERROR, "twSubscribedPropsMgr_PushProperties: spm not initialized");
		return TW_SUBSCRIBEDPROP_MGR_NOT_INTIALIZED;
	}
	if (!entityName) {
		TW_LOG(TW_ERROR, "twSubscribedPropsMgr_PushProperties: Missing parameters");
		return TW_INVALID_PARAM;
	}
	twMutex_Lock(spm->mtx);
	/* Always try to push any persisted properties first */
	if (twApi_isConnected() || (!twApi_isConnected() && forceConnect)) twSubscribedPropsMgr_SendPersistedValues();
	/* Now send any queued properties that are in RAM */
	TW_LOG(TW_TRACE, "twSubscribedPropsMgr_PushProperties: Attempting to push queued properties");
	if (!twSubscribedPropsMgr_PushPropertyList(spm->savedValues, forceConnect)) {
		/* Reset our queue size since we just emptied it */
		spm->queueSize = 0;
	}
	twMutex_Unlock(spm->mtx);
	return TW_OK;
}

int twSubscribedPropsMgr_SetPropertyVTQ(char * entityName, char * propertyName, twPrimitive * value,  DATETIME timestamp, char * quality, char fold, char pushUpdate) {
	twSubscribedProperty * p = NULL;
	ListEntry * le = NULL;
	ListEntry * leValue = NULL;
	int pushType = TW_UPDATE_ALWAYS;
	double pushThreshold = 0.0;
	char propertyFound = FALSE;
	char sendProperty = FALSE;
	int res = TW_OK;
	if (!spm || !tw_api || !tw_api->callbackList) {
		TW_LOG(TW_ERROR, "twSubscribedProps_Write: spm or api not initialized");
		if (value) twPrimitive_Delete(value);
		return TW_NULL_API_SINGLETON;
	}
	if (!entityName || !propertyName || !value) {
		TW_LOG(TW_ERROR, "twSubscribedProps_Write: Missing parameters");
		if (value) twPrimitive_Delete(value);
		return TW_INVALID_PARAM;
	}
	if (!quality) quality = "GOOD";
	/* Create a property struct */
	p = twSubscribedProperty_Create(entityName, propertyName, value, timestamp, quality, fold);
	if (!p) {
		TW_LOG(TW_ERROR, "twSubscribedProps_Write: Error allocating memory");
		twPrimitive_Delete(value);
		return TW_ERROR_ALLOCATING_MEMORY;
	}
	/* How should we handle this property? */
	/* Get the metadata from the api */
	le = twList_Next(tw_api->callbackList, NULL);
	while (le && le->value) {
		callbackInfo * cb = (callbackInfo *)le->value;
		if (cb->characteristicType == TW_PROPERTIES) {
			if (!strcmp(cb->entityName, entityName) && !strcmp(cb->charateristicName, propertyName)) {
				cJSON * j = NULL;
				twPropertyDef * def = (twPropertyDef *)cb->charateristicDefinition;
				if (!def->aspects) {
					TW_LOG(TW_ERROR, "twSubscribedProps_Write: Could not find aspects for property %s", propertyName);
					twPrimitive_Delete(value);
					return TW_INVALID_PARAM;				
				}
				j = cJSON_GetObjectItem(def->aspects, "pushType");
				if (!j || !j->valuestring) {
					TW_LOG(TW_ERROR, "twSubscribedProps_Write: Could not find pushType for property %s", propertyName);
					twPrimitive_Delete(value);
					return TW_INVALID_PARAM;				
				}
				propertyFound = TRUE;
				if (!strcmp(j->valuestring, "NEVER")) {
					pushType = TW_UPDATE_NEVER;
					break;
				} else if (!strcmp(j->valuestring, "ALWAYS")) {
					pushType = TW_UPDATE_ALWAYS;
					break;
				} else if (!strcmp(j->valuestring, "VALUE")) {
					pushType = TW_UPDATE_VALUE;
					j = cJSON_GetObjectItem(def->aspects, "pushThreshold");
					if (j && j->type == cJSON_Number) pushThreshold = j->valuedouble;
					break;
				} else if (!strcmp(j->valuestring, "ON")) {
					pushType = TW_UPDATE_ON;
					break;
				} else if (!strcmp(j->valuestring, "OFF")) {
					pushType = TW_UPDATE_OFF;
					break;
				}
			}
		}
		le = twList_Next(tw_api->callbackList, le);
	}
	if (!propertyFound) {
		TW_LOG(TW_ERROR, "twSubscribedProps_Write: Property %s metadata not found", propertyName);
		twPrimitive_Delete(value);
		return TW_SUBSCRIBED_PROPERTY_NOT_FOUND;
	}
	/* Should we push this property? */
	switch (pushType) {
	case TW_UPDATE_ALWAYS:
		sendProperty = TRUE;
		TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s pushType is ALWAYS", propertyName);
		break;
	case TW_UPDATE_NEVER:
		sendProperty = FALSE;
		TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s pushType is NEVER", propertyName);
		break;
	case TW_UPDATE_VALUE:
		/* First check to see if it has changed from the current value */
		{
		char found = FALSE;
		twSubscribedProperty * prop  = NULL;
		twSubscribedProperty * updatedVal  = NULL;
		TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s pushType is VALUE, checking change threshold", propertyName);
		leValue = twList_Next(spm->currentValues, NULL);
		while (leValue && leValue->value) {
			double current = 0;
			double newVal = 0;
			prop = (twSubscribedProperty *)leValue->value;
			if (!strcmp(prop->prop->name, propertyName)) {
				TW_LOG(TW_TRACE, "twSubscribedProps_Write: Found current value of %s", propertyName);
				found = TRUE;
				if (prop->prop->value->type == TW_INTEGER || prop->prop->value->type == TW_NUMBER) {
					if (prop->prop->value->type == TW_INTEGER) {
						current = prop->prop->value->val.integer;
						newVal = value->val.integer;
					} else {
						current = prop->prop->value->val.number;
						newVal = value->val.number;
					}
					if ((newVal > current + pushThreshold) || (newVal < current - pushThreshold)) {
						sendProperty = TRUE;
						TW_LOG(TW_TRACE, "twSubscribedProps_Write: Numeric Property %s has changed, sending to server", propertyName);
					} else {
						TW_LOG(TW_TRACE, "twSubscribedProps_Write: Numeric Property %s has changed but not by %f.  Not sending to server", 
								propertyName, pushThreshold);
					}
				} else if (!twPrimitive_Compare(prop->prop->value, p->prop->value)) {
					/* Property has changed */
					sendProperty = TRUE;
					TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s has changed, sending to server", propertyName);
				}
				break;
			}
			leValue = twList_Next(spm->currentValues, leValue);
		}
		/* Save the updated value but we need to copy it first */
		updatedVal = twSubscribedProperty_Create(entityName, propertyName, value, timestamp, quality, fold);
		if (found) {
			int res = twList_ReplaceValue(spm->currentValues, leValue, updatedVal, TRUE);
			if (res) TW_LOG(TW_WARN, "twSubscribedProps_Write: Error updating Property %s in list.  Error: %d", propertyName, res);
		} else {
			/* If this is the first value for this property we should send it and save the value locally*/
			sendProperty = TRUE;
			TW_LOG(TW_TRACE, "twSubscribedProps_Write: First write of Property %s, sending to server", propertyName);
			twList_Add(spm->currentValues, updatedVal);
		}
		break;
		}
	case TW_UPDATE_ON:
		sendProperty = twPrimitive_IsTrue(value);
		TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s is %s, pushType is ON", propertyName, sendProperty ? "ON" : "OFF");
		break;
	case TW_UPDATE_OFF:
		sendProperty = !twPrimitive_IsTrue(value);
		TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s is %s, pushType is OFF", propertyName, sendProperty ? "OFF" : "ON");
		break;
	default:
		TW_LOG(TW_ERROR, "twSubscribedProps_Write: Unknown push type %d for property %s", pushType, propertyName);
	}
	if (sendProperty) {
		int len = twSubscribedProperty_GetLength(p);
		TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s being queued to be sent to server", propertyName);
		twMutex_Lock(spm->mtx);
		if (spm->queueSize + len < twcfg.offline_msg_queue_size) {
			res = twSubscribedPropsMgr_QueueValueForSending(p, spm->savedValues);
			spm->queueSize += len;
		} else {
			TW_LOG(TW_ERROR, "twSubscribedProps_Write: Adding property %s to queue would exceed max queue size of %d", propertyName, twcfg.offline_msg_queue_size);
			twSubscribedProperty_Delete(p);
		}
		twMutex_Unlock(spm->mtx);
	} else {
		TW_LOG(TW_TRACE, "twSubscribedProps_Write: Property %s NOT being sent to server", propertyName);
		twSubscribedProperty_Delete(p);
	}
	if (pushUpdate) res = twSubscribedPropsMgr_PushSubscribedProperties(entityName, FALSE);
	twPrimitive_Delete(value);
	return res;
}


/***************************************/
/*    Subscribed Property functions    */
/***************************************/
twSubscribedProperty * twSubscribedProperty_Create (char * e, char * n, twPrimitive * v, DATETIME t, char * q, char fold) {
	twSubscribedProperty * tmp = NULL;
	if (!e || !n || !v) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_Create: NULL parameter passed in");
		return NULL;
	}
	tmp = (twSubscribedProperty *)TW_CALLOC(sizeof(twSubscribedProperty), 1);
	if (!tmp) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_Create: Error allocating memory");
		return NULL;
	}
	tmp->entity = duplicateString(e);
	tmp->prop = twPropertyVTQ_Create(n,twPrimitive_FullCopy(v),t,q);
	tmp->fold = fold;
	if (!tmp->entity || !tmp->prop) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_Create: Error allocating memory");
		twSubscribedProperty_Delete(tmp);
		return NULL;
	}
	return tmp;
}

twSubscribedProperty * twSubscribedProperty_CreateFromStream(twStream * s) {
	twSubscribedProperty * tmp = NULL;
	twPrimitive * p;
	twInfoTableRow * row = NULL;
	int i = 0;
	if (!s) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_CreateFromStream: NULL stream pointer");
		return NULL;
	}
	tmp = (twSubscribedProperty *)TW_CALLOC(sizeof(twSubscribedProperty), 1);
	if (!tmp) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_CreateFromStream: Error allocating memory");
		return NULL;
	}
	tmp->prop = (twProperty *)TW_CALLOC(sizeof(twProperty), 1);
	if (!tmp->prop) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_CreateFromStream: Error allocating memory for twProperty");
		return NULL;
	}
	/* We have stored this as an infotable row */
	row = twInfoTableRow_CreateFromStream(s);
	if (!row) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_CreateFromStream: Error creating row from stream");
		twSubscribedProperty_Delete(tmp);
		return NULL;
	}
	/* Sanity check */
	if (twInfoTableRow_GetCount(row) != 6) {
		TW_LOG(TW_ERROR,"twSubscribedProperty_CreateFromStream: Invalid primitive count in row");
		twSubscribedProperty_Delete(tmp);
		return NULL;
	}
	for (i = 0; i < 6; i++) {
		char err = FALSE;
		p = twInfoTableRow_GetEntry(row, i);
		if (!p) {
			TW_LOG(TW_ERROR,"twSubscribedProperty_CreateFromStream: Error getting primitive from row %d", i);
			twSubscribedProperty_Delete(tmp);
			return NULL;
		}
		switch (i) {
		case 0:
			if (p->type != TW_STRING) {
				err = TRUE;
				break;
			}
			tmp->entity = duplicateString(p->val.bytes.data);
			break;
		case 1:
			if (p->type != TW_STRING) {
				err = TRUE;
				break;
			}
			tmp->prop->name = duplicateString(p->val.bytes.data);
			break;
		case 2:
			tmp->prop->value = twPrimitive_FullCopy(p);
			break;
		case 3:
			if (p->type != TW_DATETIME) {
				err = TRUE;
				break;
			}
			tmp->prop->timestamp = p->val.datetime;
			break;
		case 4:
			if (p->type != TW_STRING) {
				err = TRUE;
				break;
			}
			tmp->prop->quality = duplicateString(p->val.bytes.data);
			break;
		case 5:
			if (p->type != TW_BOOLEAN) {
				err = TRUE;
				break;
			}
			tmp->fold = p->val.boolean;
			break;
		}
		if (err) {
			TW_LOG(TW_ERROR,"twSubscribedProperty_CreateFromStream: Invalid primitive type in row");
			twInfoTableRow_Delete(row);
			twSubscribedProperty_Delete(tmp);
			return NULL;
		}
	}
	twInfoTableRow_Delete(row);
	return tmp;
}

void twSubscribedProperty_Delete(void * prop) {
	twSubscribedProperty * p = (twSubscribedProperty *)prop;
	if (!prop) return;
	if (p->entity) TW_FREE((void *)p->entity);
	if (p->prop) twProperty_Delete(p->prop);
	TW_FREE(p);
}

int twSubscribedProperty_ToStream(twSubscribedProperty * p, twStream * s) {
	int res = TW_UNKNOWN_ERROR;
	twInfoTableRow * row = NULL;
	if (!p || !p->prop || !s) return TW_INVALID_PARAM;
	row = twInfoTableRow_Create(twPrimitive_CreateFromString(p->entity, TRUE));
	if (twInfoTableRow_AddEntry(row, twPrimitive_CreateFromString(p->prop->name, TRUE))) {
		twInfoTableRow_Delete(row);
		return TW_ERROR;
	}
	if (!row) return TW_ERROR_ALLOCATING_MEMORY;
	if (twInfoTableRow_AddEntry(row, twPrimitive_FullCopy(p->prop->value))) {
		twInfoTableRow_Delete(row);
		return TW_ERROR;
	}
	if (twInfoTableRow_AddEntry(row, twPrimitive_CreateFromDatetime(p->prop->timestamp))) {
		twInfoTableRow_Delete(row);
		return TW_ERROR;
	}
	if (twInfoTableRow_AddEntry(row, twPrimitive_CreateFromString(p->prop->quality, TRUE))) {
		twInfoTableRow_Delete(row);
		return TW_ERROR;
	}
	if (twInfoTableRow_AddEntry(row, twPrimitive_CreateFromBoolean(p->fold))) {
		twInfoTableRow_Delete(row);
		return TW_ERROR;
	}
	res = twInfoTableRow_ToStream(row, s);
	twInfoTableRow_Delete(row);
	return res;
}

uint32_t  twSubscribedProperty_GetLength(twSubscribedProperty * p) {
	uint32_t len = 0;
	if (!p || !p->prop || !p->entity) return UINT_MAX;
	len += strlen(p->entity);
	len += strlen(p->prop->name);
	len += strlen(p->prop->quality);
	len += sizeof(DATETIME);
	len += 1; /* fold */
	len += p->prop->value->length;
	return len;
}



/*
 *  Copyright (C) 2015 ThingWorx Inc.
 *
 *  Thingworx Subscribed Properties
 */

#include "twOSPort.h"
#include "twLogger.h"
#include "twApi.h"
#include "twInfoTable.h"

#ifndef TW_SUBSCRIBED_PROPS_H
#define TW_SUBSCRIBED_PROPS_H

#ifdef __cplusplus
extern "C" {
#endif

int twSubscribedPropsMgr_Initialize();

void twSubscribedPropsMgr_Delete();

void twSubscribedPropsMgr_SetFolding(char fold);

int twSubscribedPropsMgr_PushSubscribedProperties(char * entityName, char forceConnect);

int twSubscribedPropsMgr_SetPropertyVTQ(char * entityName, char * propertyName, twPrimitive * value,  DATETIME timestamp, char * quality, char fold, char pushUpdate);

#ifdef __cplusplus
}
#endif

#endif

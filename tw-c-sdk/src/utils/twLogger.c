/*
 *  Copyright (C) 2015 ThingWorx Inc.
 *
 *  Logging facility
 */

#include "twLogger.h"
#include "twOSPort.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

twLogger * logger_singleton = NULL;

char * levelString(enum LogLevel level) {
	switch(level) {
	case TW_TRACE: 
		return "TRACE";
	case TW_DEBUG: 
		return "DEBUG";
	case TW_INFO: 
		return "INFO";
	case TW_WARN: 
		return "WARN";
	case TW_ERROR: 
		return "ERROR";
	case TW_FORCE: 
		return "FORCE";
	case TW_AUDIT: 
		return "AUDIT";
	default: 
		return "UNKNOWN";
	}
}

twLogger * twLogger_Instance() {
	twLogger * temp = NULL;
	/* Check to see if it was created already */
	if (logger_singleton) return logger_singleton;
	temp = (twLogger *)TW_CALLOC(sizeof(twLogger),1);
	temp->level = TW_WARN;
	temp->f = LOGGING_FUNCTION;
	temp->isVerbose = 0;
	temp->buffer = (char *)TW_MALLOC(TW_LOGGER_BUF_SIZE);
	temp->mtx = twMutex_Create();
	logger_singleton = temp;
	return logger_singleton;
}

int twLogger_Delete() {
	if (logger_singleton) {
		twLogger * temp = logger_singleton;
		logger_singleton = NULL;
		TW_FREE(temp->buffer);
		twMutex_Delete(temp->mtx);
		TW_FREE(temp);
	}
	return TW_OK;
}

int twLogger_SetLevel(enum LogLevel level) {
	twLogger * l = twLogger_Instance();
	if (l) {
		l->level = level;
		return TW_OK;
	} else return TW_NULL_OR_INVALID_LOGGER_SINGLETON;
}

int twLogger_SetFunction(log_function f) {
	twLogger * l = twLogger_Instance();
	if (l) {
		l->f = f;
		return TW_OK;
	} else return TW_NULL_OR_INVALID_LOGGER_SINGLETON;
}

int twLogger_SetIsVerbose(char val) {
	twLogger * l = twLogger_Instance();
	if (l) {
		l->isVerbose = val;
		return TW_OK;
	} else return TW_NULL_OR_INVALID_LOGGER_SINGLETON;
}

void twLog(enum LogLevel level, const char * format, ... ) {
	char timeStr[80];
	twLogger * l = twLogger_Instance();
	va_list va;

	if (!l || level < l->level) return;
	/* prepare the message */
	if (!l->buffer || !l->mtx) return;
	twMutex_Lock(l->mtx);
    va_start(va, format);
    vsnprintf(l->buffer, TW_LOGGER_BUF_SIZE - 1, format, va);
	va_end(va);
	/* get the timestamp */
	twGetSystemTimeString(timeStr, "%Y-%m-%d %H:%M:%S", 80, 1, 1);
	l->f(level, timeStr, l->buffer);
	twMutex_Unlock(l->mtx);
}

/**************************************/
/*        For Verbose debugging       */
/**************************************/
#include "twMessages.h"

char * twCodeToString(enum msgCodeEnum m) {
	switch(m) {
	case	TWX_GET:
		return "GET";
	case	TWX_PUT:
		return "PUT";
	case	TWX_POST:
		return "POST";
	case	TWX_DEL:
		return "DEL";
	case	TWX_BIND:
		return "BIND";
	case	TWX_UNBIND:
		return "UNBIND";
	case	TWX_AUTH:
		return "AUTH";
	case	TWX_KEEP_ALIVE:
		return "KEEP_ALIVE";
	case	TWX_SUCCESS:
		return "SUCCESS";
	case	TWX_BAD_REQUEST:
		return "BAD_REQUEST";
	case	TWX_UNAUTHORIZED:
		return "UNAUTHORIZED";
	case	TWX_BAD_OPTION:
		return "BAD_OPTION";
	case	TWX_FORBIDDEN:
		return "FORBIDDEN";
	case	TWX_NOT_FOUND:
		return "NOT_FOUND";
	case	TWX_METHOD_NOT_ALLOWED:
		return "METHOD_NOT_ALLOWED";
	case	TWX_NOT_ACCEPTABLE:
		return "NOT_ACCEPTABLE";
	case	TWX_PRECONDITION_FAILED:
		return "PRECONDITION_FAILED";
	case	TWX_ENTITY_TOO_LARGE:
		return "ENTITY_TOO_LARGE";
	case	TWX_UNSUPPORTED_CONTENT_FORMAT:
		return "UNSUPPORTED_CONTENT_FORMAT";
	case	TWX_INTERNAL_SERVER_ERROR:
		return "INTERNAL_SERVER_ERROR";
	case	TWX_NOT_IMPLEMENTED:
		return "NOT_IMPLEMENTED";
	case	TWX_BAD_GATEWAY:
		return "BAD_GATEWAY";
	case	TWX_SERVICE_UNAVAILABLE:
		return "SERVICE_UNAVAILABLE";
	case	TWX_GATEWAY_TIMEOUT:
		return "GATEWAY_TIMEOUT";
	default:
		return "UNKNOWN";
	};
}

char * twEntityToString(enum entityTypeEnum m) {
	switch(m) {
	case	TW_THING:
		return "THING";
	case	TW_RESOURCE:
		return "RESOURCE";
	case	TW_SUBSYSTEM:
		return "SUBSYSTEM";
	default:
		return "UNKNOWN";
	};
}

char * twCharacteristicToString(enum characteristicEnum m) {
	switch(m) {
	case	TW_PROPERTIES:
		return "PROPERTIES";
	case	TW_SERVICES:
		return "SERVICES";
	case	TW_EVENTS:
		return "EVENTS";
	default:
		return "UNKNOWN";
	};
}
void logAuthBody( twAuthBody * b, char * buf, int32_t maxLength) {
	if (b && maxLength > 1) snprintf(buf, maxLength - 1, "Claim Count: 1\nName: %s\nValue: %s\n", b->name, b->value);
}

void logBindBody( twBindBody * b, char * buf, int32_t maxLength) {
	if (b && maxLength > 1) {
		ListEntry * le = NULL;
		snprintf(buf, maxLength - 1, "Count: %d\n", b->count);
		maxLength = maxLength - strlen(buf);
		if (maxLength <= 1) return;
		buf = buf + strlen(buf);
		if (b->gatewayName && b->gatewayType) {
			snprintf(buf, maxLength - 1, "GatewayName: %s\nGatewayType:%s\n", b->gatewayName, b->gatewayType);
			maxLength = maxLength - strlen(buf);
			if (maxLength <= 1) return;
			buf = buf + strlen(buf);
		}
		le = twList_Next(b->names, NULL);
		while (le && le->value && maxLength > 0) {
			snprintf(buf, maxLength - 1, "Name: %s\n", (char *)le->value);
			maxLength = maxLength - strlen(buf);
			if (maxLength <= 1) return;
			buf = buf + strlen(buf);
			le = twList_Next(b->names, le);
		}
	}
}

void logInfoTable(twInfoTable * it, char * buf, int32_t maxLength);
void logPrimitive(twPrimitive * p, char * buf, int32_t maxLength) {
	char * tmp = NULL;
	if (!p || !buf) return;
	snprintf(buf, maxLength - 1, "   BaseType: %9s\t", baseTypeToString(p->type));
	maxLength = maxLength - strlen(buf);
	if (maxLength <= 1) return;
	buf = buf + strlen(buf);
	tmp = (char *)TW_CALLOC(maxLength, 1);
	if (!tmp) {
		snprintf(buf, maxLength - 1, "ERROR ALLOCATING BUFFER FOR LOGGING ");
		return;
	}
	switch (p->typeFamily) {
	case 	TW_NOTHING:
		break;
	case 	TW_BLOB:
		{
		snprintf(buf, maxLength - 1, "Value: IMAGE/BLOB\tLength: %d\n", p->val.bytes.len);
		break;
		}
	case	TW_STRING:
		{
		snprintf(buf, maxLength - 1, "Value: %s\tLength: %d\n", p->val.bytes.data, p->val.bytes.len);
		break;
		}
	case	TW_VARIANT:
		{
		snprintf(buf, maxLength - 1, "Value: ");
		maxLength = maxLength - strlen(buf);
		if (maxLength <= 1) break;
		buf = buf + strlen(buf);
		logPrimitive(p->val.variant, buf, maxLength);
		break;
		}
	case	TW_NUMBER:
		{
		snprintf(buf, maxLength - 1, "Value: %f\n", p->val.number);
		break;
		}
	case	TW_INTEGER:
		{
		snprintf(buf, maxLength - 1, "Value: %d\n", p->val.integer);
		break;
		}
	case	TW_BOOLEAN:
		{
		snprintf(buf, maxLength - 1, "Value: %s\n", p->val.boolean ? "TRUE" : "FALSE");
		break;
		}
	case	TW_DATETIME:
		{
		snprintf(buf, maxLength - 1, "Value: %llu\n", (unsigned long long)(p->val.datetime));
		break;
		}
	case	TW_LOCATION:
		{
		snprintf(buf, maxLength - 1, "Value: Long: %f, Lat: %f, Elev: %f\n", 
			p->val.location.longitude,p->val.location.latitude,p->val.location.elevation);
		break;
		}
	case	TW_INFOTABLE:
		{
		snprintf(buf, maxLength - 1, "Value:\n");
		maxLength = maxLength - strlen(buf);
		if (maxLength <= 1) break;
		buf = buf + strlen(buf);
		logInfoTable(p->val.infotable, buf, maxLength);
		break;
		}
	default:
		snprintf(buf, maxLength - 1, "INVALID BASE TYPE\n");
		break;
	}
	if (tmp) TW_FREE(tmp);
}

void logInfoTable(twInfoTable * it, char * buf, int32_t maxLength) {	
	if (it && it->ds && it->ds ->entries && maxLength > 1) {
		ListEntry * le = NULL;
		snprintf(buf, maxLength - 1, "DataShape:\n");
		maxLength = maxLength - strlen(buf);
		if (maxLength <= 1) return;
		buf = buf + strlen(buf);
		le = twList_Next(it->ds ->entries, NULL);
		while (le && le->value && maxLength > 0) {
			twDataShapeEntry * dse = (twDataShapeEntry *)le->value;
			snprintf(buf, maxLength - 1, "   Name: %16s\tBaseType: %s\n", 
				dse->name, baseTypeToString(dse->type));
			maxLength = maxLength - strlen(buf);
			if (maxLength <= 1) return;
			buf = buf + strlen(buf);
			le = twList_Next(it->ds ->entries, le);
		}
		if (it->rows && maxLength > 0) {
			int i = 1;
			le = twList_Next(it->rows, NULL);
			while (le && le->value && maxLength > 0) {
				twInfoTableRow * row = (twInfoTableRow *)le->value;
				snprintf(buf, maxLength - 1, "Row %d:\n", i++);
				maxLength = maxLength - strlen(buf);
				if (maxLength <= 1) return;
				buf = buf + strlen(buf);
				if (row->fieldEntries) {
					ListEntry * field = NULL;
					field = twList_Next(row->fieldEntries, NULL);
					while (field && field->value && maxLength > 0) {
						twPrimitive * p = (twPrimitive *)field->value;
						logPrimitive(p, buf, maxLength);
						maxLength = maxLength - strlen(buf);
						if (maxLength <= 1) return;
						buf = buf + strlen(buf);
						field = twList_Next(row->fieldEntries, field);
					}
				}
				le = twList_Next(it->rows, le);
			}
		}
	}
}

void logRequestBody( twRequestBody * b, char * buf, int32_t maxLength) {
	if (b && maxLength > 1) {
		snprintf(buf, maxLength - 1, "EntityType: %s\nEntityName: %s\nCharacteristicType: %s\nCharateristicName: %s%s", 
			twEntityToString(b->entityType),b->entityName,twCharacteristicToString(b->characteristicType),b->characteristicName,  b->numHeaders ? "Headers:\n":"\n");
		maxLength = maxLength - strlen(buf);
		if (maxLength <= 1) return;
		buf = buf + strlen(buf);
		if (b->numHeaders && b->headers) {
			ListEntry * le = NULL;
			le = twList_Next(b->headers, NULL);
			while (le && le->value && maxLength > 0) {
				twHeader * h = (twHeader *)le->value;
				snprintf(buf, maxLength - 1, "   Name: %s\n   Value: %s\n", 
					h->name ? h->name : "NULL", h->value? h->value : "NULL");
				maxLength = maxLength - strlen(buf);
				if (maxLength <= 1) return;
				buf = buf + strlen(buf);
				le = twList_Next(b->headers, le);
			}
		}
		snprintf(buf, maxLength - 1, "Parameter Type: %s\n", b->params ? "INFOTABLE" : "NONE");
		maxLength = maxLength - strlen(buf);
		if (maxLength <= 1) return;
		buf = buf + strlen(buf);
		if (b->params && maxLength > 1) {
			logInfoTable(b->params, buf, maxLength);
		}
	}
}

void logResponseBody( twResponseBody * b, char * buf, int32_t maxLength) {
	if (b && maxLength > 1) {
		if (b->reason) {
			snprintf(buf, maxLength - 1, "Reason: %s\n", b->reason);
			maxLength = maxLength - strlen(buf);
			if (maxLength <= 1) return;
			buf = buf + strlen(buf);
		}
		snprintf(buf, maxLength - 1, "Result Type: %s\n", baseTypeToString(b->contentType));
		maxLength = maxLength - strlen(buf);
		if (maxLength <= 1) return;
		buf = buf + strlen(buf);
		if (b->contentType == TW_INFOTABLE && b->content && maxLength > 0) {
			logInfoTable(b->content, buf, maxLength);
		}
	}
}

void twLogMessage (void * msg, char * preamble) {
	char buf[TW_LOGGER_BUF_SIZE];
	twMessage * m = (twMessage *)msg;
	int32_t bytesUsed = 0;
	twLogger * l = twLogger_Instance();
	if (!m|| !l || !l->isVerbose || TW_TRACE < l->level) return;
	memset(buf, 0, TW_LOGGER_BUF_SIZE);
	/* Append an indicator that we haven't logged the complete message, just in case */
	strcpy(&buf[TW_LOGGER_BUF_SIZE - 6], "<...>");
	bytesUsed = 6;
	sprintf(buf, "%s\nMessage Details:\nVersion: %d\nMethod/Code: 0x%x (%s)\nRequestID: %d\nEndpointID:%d\nSessionID: %d\nMultipart: %d\n",
		preamble ? preamble : "", m->version,m->code, twCodeToString(m->code),m->requestId,m->endpointId,m->sessionId,m->multipartMarker);
	bytesUsed = strlen(buf);
	switch (m->type) {
	case TW_AUTH:
		logAuthBody((twAuthBody *)m->body, buf + bytesUsed, TW_LOGGER_BUF_SIZE - bytesUsed - 1);
		break;
	case TW_BIND:
		logBindBody((twBindBody *)m->body, buf + bytesUsed, TW_LOGGER_BUF_SIZE - bytesUsed - 1);
		break;
	case TW_REQUEST:
		logRequestBody((twRequestBody *)m->body, buf + bytesUsed, TW_LOGGER_BUF_SIZE - bytesUsed - 1);
		break;
	case TW_RESPONSE:
		logResponseBody((twResponseBody *)m->body, buf + bytesUsed, TW_LOGGER_BUF_SIZE - bytesUsed - 1);
		break;
	default:
		break;
	}
	TW_LOG(TW_TRACE, "%s", buf);
}

void twLogHexString(const char * msg, char * preamble, int32_t length) {
	char * tmp = NULL;
	char hex[4];
	int i;
	uint16_t size;
	int32_t newLength = length;
	twLogger * l = twLogger_Instance();
	if (!msg || !preamble || !l || !l->isVerbose || TW_TRACE < l->level) return;
	/* prepare the message */
	size = length * 3 + 1;
	if (size > TW_LOGGER_BUF_SIZE) {
		size = TW_LOGGER_BUF_SIZE;
		newLength = size/3 - 1;
	}
	tmp = (char *)TW_CALLOC(size, 1);
	if (tmp) {
		for (i = 0; i < newLength; i++) {
			sprintf(hex, "%02X ", (unsigned char)msg[i]);
			strcat(tmp, hex);
		}
		TW_LOG(TW_TRACE, "%s%s", preamble, tmp);
		TW_FREE(tmp);
	} else TW_LOG(TW_ERROR, "twLogHexString: Error allocating storage");
}


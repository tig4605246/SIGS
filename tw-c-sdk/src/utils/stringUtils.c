/*
 *  Copyright (C) 2015 ThingWorx Inc.
 *
 *  String Utils
 */

#include "twOSPort.h"
#include "string.h"

INLINE char locase(char c) 
    { if (c >= 'A' && c <= 'Z') return (char)(c + 32); return c; }

INLINE char upcase(char c) 
    { if (c >= 'a' && c <= 'z') return (char)(c - 32); return c; }

char * lowercase(char *input) {
	if (input != 0) {
		char * x = input;
        while (*x != 0) {
            *x = locase(*x);
			x++;
		}
    }
	return input;
}

char * uppercase(char *input) {
	if (input != 0) {
		char * x = input;
        while (*x != 0) {
            *x = upcase(*x);
			x++;
		}
    }
	return input;
}

char * duplicateString(const char * input) {
	char * res = NULL;
	if (!input) return NULL;
	res = (char *)TW_CALLOC(strlen(input) + 1, 1);
	if (res) strcpy(res, input);
	return res;
}

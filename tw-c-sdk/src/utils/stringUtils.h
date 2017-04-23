/***************************************
 *  Copyright (C) 2015 ThingWorx Inc.  *
 ***************************************/

/**
 * \file stringUtils.h
 * \brief String utility function prototypes.
*/

#ifndef TW_STRING_UTILS_H
#define TW_STRING_UTILS_H

#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Converts a string to lowercase.
 *
 * \param[in]     input     The string to change to lowercase.
 *
 * \return The lowercase format of \p input.
 *
 * \note The string is directly modified, not copied.
*/
char * lowercase(char *input);

/**
 * \brief Converts a string to uppercase.
 *
 * \param[in]     input     The string to change to uppercase.
 *
 * \return The uppercase format of \p input.
 *
 * \note The string is directly modified, not copied.
*/
char * uppercase(char *input);

/**
 * \brief Copies a string.
 *
 * \param[in]     input     The string to copy.
 *
 * \return A copy of \p input.
 *
 * \note The calling function gains ownership of the returned string and
 * retains ownership of the \p input string.
*/
char * duplicateString(const char * input);

#ifdef __cplusplus
}
#endif

#endif

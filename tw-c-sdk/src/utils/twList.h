/***************************************
 *  Copyright (C) 2015 ThingWorx Inc.  *
 ***************************************/

/**
 * \file list.h
 * \brief Linked List definitions and function prototypes
 *
 * Contains structure type definitions and function prototypes for the
 * ThingWorx linked list implementation.  ::twList is dynamically sized,
 * thread-safe, untyped, and doubly linked.
*/

#ifndef TW_LIST_H
#define TW_LIST_H

#include "twOSPort.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ListEntry;

/**
 * \brief Signature of a function called to delete the values in a ::ListEntry.
 *
 * \param[in]     item      A pointer to an item to be deleted.
 *
 * \return Nothing.
*/
typedef void (*del_func) (void * item);

/**
 * \brief Linked list entry structure definition.
*/
typedef struct ListEntry {
    struct ListEntry *next; /**< A pointer to the next ::ListEntry in the ::twList. **/
    struct ListEntry *prev; /**< A pointer to the previous ::ListEntry in the ::twList. **/
    void *value;            /**< The data item of this ::ListEntry. **/
} ListEntry;

/**
 * \brief Linked list structure definition.
*/
typedef struct twList {
	int count;                /**< The number of elements in the linked list. **/
	struct ListEntry *first;  /**< A pointer to the first ::ListEntry in the ::twList. **/
	struct ListEntry *last;   /**< A pointer to the last ::ListEntry in the ::twList. **/
	TW_MUTEX mtx;             /**< A mutex for this ::twList structure. **/
	del_func delete_function; /**< A deletion function associated with this ::twList. **/
} twList;

/**
 * \brief Creates a new ::twList.
 *
 * \param[in]     delete_function   A pointer to a function to call when
 *                                   deleting a ::ListEntry value.  If NULL is
 *                                   passed, the default function free() is
 *                                   used.
 *
 * \return A pointer to the newly allocated ::twList structure.  Returns NULL if
 * an error was encountered.
 *
 * \note The new ::twList gains ownership of all ::ListEntry and
 * ::ListEntry#value pointers in the ::ListEntry.
 * \note The calling function gains ownership of the returned ::twList and is
 * responsible for deleting it via twList_Delete().
*/
twList * twList_Create(del_func delete_function);

/**
 * \brief Frees all memory associated with a ::twList and all its owned
 * substructures.
 *
 * \param[in]     list      A pointer to the ::twList to delete.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twList_Delete(struct twList *list);

/**
 * \brief Deletes all ::ListEntry items within a ::twList and frees all memory
 * associated with them.
 *
 * \param[in]     list      A pointer to the ::twList to clear.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twList_Clear(struct twList *list);

/**
 * \brief Creates a new ::ListEntry and adds it to a ::twList.
 *
 * \param[in]     list      A pointer to the ::twList to add the ::ListEntry
 *                          to.
 * \param[in]     value     The value to be assigned to the ::ListEntry.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twList_Add(twList *list, void *value);

/**
 * \brief Removes a ::ListEntry from a ::twList and frees all memory associated
 * it (the value pointer may be optionally freed via the \p deleteValue
 * parameter).
 *
 * \param[in]     list          A pointer to the ::twList to remove the
 *                              ::ListEntry from.
 * \param[in]     entry         A pointer to the ::ListEntry to remove.
 * \param[in]     deleteValue   If TRUE, the value of the ::ListItem will be
 *                              deleted via the ::twList#delete_function
 *                              associated with the ::twList.  If FALSE, the
 *                              value is not deleted.
 *
 * \return #TW_OK if successful, positive integral on error code (see
 * twErrors.h) if an error was encountered.
*/
int twList_Remove(struct twList *list, struct ListEntry * entry, char deleteValue);

/**
 * \brief Gets the next ::ListEntry in a ::twList via the ::ListEntry#next
 * pointer.
 *
 * \param[in]     list      A pointer to the ::twList to operate on.
 * \param[in]     entry     A pointer to the current entry (NULL will get
 *                          ::twList#first entry).
 *
 * \return A pointer to the next ::ListEntry in the ::twList.  Returns NULL if
 * \p entry was the last ::ListEntry or if an error was encountered.
 *
 * \note \p list will maintain ownership of the \p entry pointer so the calling
 * function should <b>not</b> delete it.
*/
ListEntry * twList_Next(struct twList *list, struct ListEntry * entry);

/**
 * \brief Given an \p index, gets the ::ListEntry associated with that index.
 *
 * \param[in]     list      A pointer to the ::twList to get the ::ListEntry
 *                          from.
 * \param[in]     index     The (zero-based) index of the ::ListEntry to
 *                          retrieve.
 *
 * \return A pointer to the ::ListEntry associated with \p index of \p list.
 *
 * \note \p list will maintain ownership of the \p entry pointer so the calling
 * function should <b>not</b> delete it.
*/
ListEntry * twList_GetByIndex(struct twList *list, int index);

/**
 * \brief Gets the number of entries in a list via ::twList#count.
 *
 * \param[in]     list      A pointer to the ::twList to get the number of
 *                          entries of.
 * 
 * \return The number of entries in the list (0 if \p list is NULL).
*/
int twList_GetCount(struct twList *list);

/*
twList_ReplaceValue - Replaces the value of the specified list entry with the new value supplied.
Parameters:
    list - pointer to the list to operate on
	entry - pointer to the entry whose value should be replaced.  
	new_value - the new value
	dispose - Boolean: delete the old value using the delete function specified when the list was created 
Return:
	int - zero if successful, non-zero if an error occurred
*/
int twList_ReplaceValue(struct twList *list, struct ListEntry * entry, void * new_value, char dispose);

#ifdef __cplusplus
}
#endif

#endif

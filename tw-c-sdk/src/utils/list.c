/*
 *  Copyright (C) 2015 ThingWorx Inc.
 *
 *  Doubly linked list utilities
 */

#include "twList.h"
#include "twOSPort.h"

twList * twList_Create(del_func delete_function) {
	twList * list = (twList *)TW_CALLOC(sizeof(twList), 1);
	if (list) {
		list->mtx = twMutex_Create();
		if (!list->mtx) {
			twList_Delete(list);
			list = 0;
		}
		list->delete_function = delete_function;
	}
	return list;
}

int twList_Delete(struct twList * list) {
	if (list) {
		twList_Clear(list);
		twMutex_Delete(list->mtx);
		TW_FREE(list);
		return TW_OK;
	}
	return TW_INVALID_PARAM;
}


int twList_Clear(struct twList *list) {
	struct ListEntry * entry = NULL;
	if (list) {
		twMutex_Lock(list->mtx);
		entry = list->first;
		while (entry) {
			ListEntry * tmp = entry->next;
			if (list->delete_function) {
				list->delete_function(entry->value);
			} else TW_FREE(entry->value);
			TW_FREE(entry);
			entry = tmp;
		}
		list->count = 0;
		list->first = NULL;
		list->last = NULL;
		twMutex_Unlock(list->mtx);
		return TW_OK;
	}
	return TW_INVALID_PARAM;
}

int twList_Add(struct twList *list, void *value) {
	struct ListEntry * newEntry = NULL;
	if (list) {
		newEntry = (ListEntry *)TW_CALLOC(sizeof(ListEntry), 1);
		if (!newEntry) return TW_ERROR_ALLOCATING_MEMORY;
		newEntry->value = value;
		/* Find the last entry */
		twMutex_Lock(list->mtx);
		if (!list->first) {
			/* This will be the first entry in the list */
			list->first = newEntry;
			list->last = newEntry;
			newEntry->prev = NULL;
			newEntry->next = NULL;
		} else {
			newEntry->prev = list->last;
			newEntry->next = NULL;
			list->last->next = newEntry;
			list->last = newEntry;
		}
		list->count++;
		twMutex_Unlock(list->mtx);
		return TW_OK;
	}
	return TW_INVALID_PARAM;
}

int twList_Remove(struct twList *list, struct ListEntry * entry, char deleteValue) {
	struct ListEntry * node = NULL;
	void * val = NULL;
	if (!list || !entry) return TW_INVALID_PARAM;
	/* find the entry */
	twMutex_Lock(list->mtx);
	node = list->first;
	while (node) {
		if (node == entry) { 
			val = node->value;
			if (node == list->first) list->first = node->next;
			if (node == list->last) list->last = node->prev;
			if (node->prev) node->prev->next = node->next;
			if (node->next) node->next->prev = node->prev;
			break;
		}
		node = node->next;
	}
	if (deleteValue && val) {
		if (list->delete_function) {
			list->delete_function(val);
		} else TW_FREE(val);
	}
	TW_FREE (entry);
	list->count--;
	twMutex_Unlock(list->mtx);
	return TW_OK;
}

ListEntry * twList_Next(twList *list, ListEntry * entry) {
	struct ListEntry * node = NULL;
	if (!list || list->count == 0) return NULL;
	/* find the entry */
	twMutex_Lock(list->mtx);
	node = list->first;
	/* If entry is NULL just return the first entry in the list */
	/* seems simple, but if this or the "next" element has been removed, it isn't */
	if (entry) {
		while (node) {
			if (node == entry) { 
				/* the passed in entry still exists */
				node = node->next;
				break;
			}
			node = node->next;
		}
	}
	twMutex_Unlock(list->mtx);
	return node;
}

ListEntry * twList_GetByIndex(struct twList *list, int index) {
	ListEntry * le = NULL;
	int count = 0;
	if (!list) return NULL;
	if (index >= list->count) return NULL;
	le = twList_Next(list,NULL);
	while (le) {
		if (count++ == index) return le;
		le = twList_Next(list,le);
	}
	return NULL;
}

int twList_GetCount(struct twList *list) {
	int count = 0;
	if (list) {
		ListEntry * le = NULL;
		le = twList_Next(list,NULL);
		while (le) {
			count++;
			le = twList_Next(list,le);
		}
	}
	return count;
}

int twList_ReplaceValue(struct twList *list, struct ListEntry * entry, void * new_value, char dispose) {
	struct ListEntry * node = NULL;
	if (!list || list->count == 0 || !entry) return TW_INVALID_PARAM;
	/* find the entry */
	twMutex_Lock(list->mtx);
	node = list->first;
	/* If entry is NULL just return the first entry in the list */
	/* seems simple, but if this or the "next" element has been removed, it isn't */
	if (entry) {
		while (node) {
			if (node == entry) { 
				if (dispose) {
					if (list->delete_function) list->delete_function(node->value);
					else TW_FREE(node->value);
				}
				node->value = new_value;
				return TW_OK;
			}
			node = node->next;
		}
	}
	twMutex_Unlock(list->mtx);
	return TW_LIST_ENTRY_NOT_FOUND;
}

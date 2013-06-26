/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Liexusong <280259971@qq.com>                                 |
  +----------------------------------------------------------------------+
*/

#include <stdlib.h>
#include "beast_mm.h"
#include "beast_lock.h"
#include "php.h"
#include "cache.h"


#define BUCKETS_DEFAULT_SIZE 1021


static int beast_cache_initialization = 0;
static cache_item_t **beast_cache_buckets = NULL;
static beast_locker_t beast_cache_locker;


static int beast_cache_hash(cache_key_t *key)
{
	return key->device * 3 + key->inode * 7;
}


int beast_cache_init(int size)
{
	int index;
	
	if (beast_cache_initialization) {
		return 0;
	}
	
	if (beast_mm_init(size) == -1) {
		return -1;
	}
	
	beast_cache_locker = beast_locker_create();
	if (beast_cache_locker == -1) {
		return -1;
	}
	
	beast_cache_buckets = beast_mm_malloc(sizeof(cache_item_t *) * BUCKETS_DEFAULT_SIZE);
	if (!beast_cache_buckets) {
		return -1;
	}
	
	for (index = 0; index < BUCKETS_DEFAULT_SIZE; index++) {
		beast_cache_buckets[index] = NULL;
	}
	
	beast_cache_initialization = 1;
	
	return 0;
}


cache_item_t *beast_cache_find(cache_key_t *key)
{
	int hashval = beast_cache_hash(key);
	int index = hashval % BUCKETS_DEFAULT_SIZE;
	cache_item_t *item, *temp;
	
	beast_locker_lock(beast_cache_locker);
	
	item = beast_cache_buckets[index];
	while (item) {
		if (item->key.device == key->device &&
		      item->key.inode == key->inode)
		{
			break;
		}
		item = item->next;
	}

	if (item && item->key.mtime < key->mtime) /* cache exprie */
	{
		temp = beast_cache_buckets[index];
		if (temp == item) { /* first node */
			beast_cache_buckets[index] = NULL;
		} else {
			while (temp->next != item)
				temp = temp->next;
			temp->next = item->next;
		}

		beast_mm_free(item);
		item = NULL;
	}
	
	beast_locker_unlock(beast_cache_locker);
	
	return item;
}


cache_item_t *beast_cache_create(cache_key_t *key, int size)
{
	cache_item_t *item, *next;
	int i, msize;
	
	msize = sizeof(*item) + size;
	
	item = beast_mm_malloc(msize);
	if (!item)
	{
		beast_locker_lock(beast_cache_locker);
		
		for (i = 0; i < BUCKETS_DEFAULT_SIZE; i++) {
			if (beast_mm_availspace() >= msize) {
				break;
			}
			
			item = beast_cache_buckets[i];
			while (item) {
				next = item->next;
				beast_mm_free(item);
				item = next;
			}
			beast_cache_buckets[i] = NULL;
		}
		
		beast_locker_unlock(beast_cache_locker);
		
		item = beast_mm_malloc(msize);
		if (!item) {
			return NULL;
		}
	}

	item->key.device = key->device;
	item->key.inode = key->inode;
	item->key.fsize = key->fsize;
	item->key.mtime = key->mtime;
	item->next = NULL;

	return item;
}


/*
 * Push cache item into cache manager,
 * this function return a cache item,
 * may be return value not equals push item,
 * so we must use return value.
 */
cache_item_t *beast_cache_push(cache_item_t *item)
{
	int hashval = beast_cache_hash(&item->key);
	int index = hashval % BUCKETS_DEFAULT_SIZE;
	cache_item_t **this;
	
	beast_locker_lock(beast_cache_locker);
	
	this = &beast_cache_buckets[index];
	while (*this) {
		/* this item was exists */
		if (!memcmp(&(*this)->key, &item->key, sizeof(cache_key_t))) {
			beast_mm_free(item);
			item = *this;
			break;
		}
		this = &(*this)->next;
	}
	
	*this = item;
	
	beast_locker_unlock(beast_cache_locker);
	
	return item;
}


int beast_cache_destroy()
{
	int index;
	cache_item_t *item, *next;
	
	if (!beast_cache_initialization) {
		return 0;
	}
	
	beast_locker_lock(beast_cache_locker);
	
	for (index = 0; index < BUCKETS_DEFAULT_SIZE; index++) {
		item = beast_cache_buckets[index];
		while (item) {
			next = item->next;
			beast_mm_free(item);
			item = next;
		}
	}
	beast_mm_free(beast_cache_buckets);
	
	beast_mm_destroy();
	
	beast_locker_unlock(beast_cache_locker);
	beast_locker_destroy(beast_cache_locker);
	
	beast_cache_initialization = 0;
	
	return 0;
}


void beast_cache_info(zval *retval)
{
	char key[128];
	int i;
	cache_item_t *item;
	
	beast_locker_lock(beast_cache_locker);
	
	for (i = 0; i < BUCKETS_DEFAULT_SIZE; i++) {
		item = beast_cache_buckets[i];
		while (item) {
			sprintf(key, "{device(%d)#inode(%d)}", item->key.device, item->key.inode);
			add_assoc_long(retval, key, item->key.fsize);
			item = item->next;
		}
	}
	
	beast_locker_unlock(beast_cache_locker);
}

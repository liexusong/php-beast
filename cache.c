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
#include <sys/types.h>
#include <sys/mman.h>

#include "beast_mm.h"
#include "beast_lock.h"
#include "php.h"
#include "cache.h"
#include "beast_log.h"


#define BUCKETS_DEFAULT_SIZE 1021


static int beast_cache_initialization = 0;
static cache_item_t **beast_cache_buckets = NULL;
static beast_locker_t *beast_cache_locker;


static inline int beast_cache_hash(cache_key_t *key)
{
    return key->device * 3 + key->inode * 7;
}


int beast_cache_init(int size)
{
    int index, bucket_size;
    char lock_file[512];

    if (beast_cache_initialization) {
        return 0;
    }

    if (beast_mm_init(size) == -1) {
        return -1;
    }

    sprintf(lock_file, "%s/beast.clock", beast_lock_path);

    beast_cache_locker = beast_locker_create(lock_file);
    if (beast_cache_locker == NULL) {
        beast_write_log(beast_log_error, "Unable create cache "
                                         "locker for beast");
        beast_mm_destroy();
        return -1;
    }

    bucket_size = sizeof(cache_item_t *) * BUCKETS_DEFAULT_SIZE;
    beast_cache_buckets = (cache_item_t **)mmap(NULL, bucket_size,
                              PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
    if (!beast_cache_buckets) {
        beast_write_log(beast_log_error, "Unable alloc memory for beast");
        beast_locker_destroy(beast_cache_locker);
        beast_mm_destroy();
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
    
    beast_locker_rdlock(beast_cache_locker); /* read lock */
    
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
        if (temp == item) { /* the header node */
            beast_cache_buckets[index] = item->next;
        } else {
            while (temp->next != item) /* find prev node */
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
    int i, msize, bsize;

    msize = sizeof(*item) + size;
    bsize = sizeof(cache_item_t *) * BUCKETS_DEFAULT_SIZE;

    if ((msize + bsize) > beast_mm_realspace()) {
        beast_write_log(beast_log_error, "Cache item size too big");
        return NULL;
    }

    item = beast_mm_malloc(msize);
    if (!item)
    {
        int index;

        /* clean all caches */

        beast_locker_lock(beast_cache_locker);

        for (index = 0; index < BUCKETS_DEFAULT_SIZE; index++) {
            beast_cache_buckets[index] = NULL;
        }

        beast_locker_unlock(beast_cache_locker);

        beast_mm_flush();

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
    cache_item_t **this, *self;
    
    beast_locker_lock(beast_cache_locker); /* lock */

#if 0
    this = &beast_cache_buckets[index];
    while (*this) {
        self = *this;
        /* the same files */
        if (self->key.device == item->key.device &&
             self->key.inode == item->key.inode)
        {
            if (self->key.mtime >= item->key.mtime) {
                beast_mm_free(item);
                beast_locker_unlock(beast_cache_locker); /* unlock */
                return self;
            } else { /* do replace */
                item->next = self->next;
                beast_mm_free(self);
                *this = item;
                beast_locker_unlock(beast_cache_locker); /* unlock */
                return item;
            }
        }
        this = &self->next;
    }

    *this = item;
#endif

    item->next = beast_cache_buckets[index];
    beast_cache_buckets[index] = item;

    beast_locker_unlock(beast_cache_locker); /* unlock */
    
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

#if 0  /* not need free the item cache, because beast_mm_destroy() would free */
    for (index = 0; index < BUCKETS_DEFAULT_SIZE; index++) {
        item = beast_cache_buckets[index];
        while (item) {
            next = item->next;
            beast_mm_free(item);
            item = next;
        }
    }

    beast_mm_free(beast_cache_buckets);
#endif

    beast_mm_destroy();

    /* free cache buckets's mmap memory */
    munmap(beast_cache_buckets, sizeof(cache_item_t *) * BUCKETS_DEFAULT_SIZE);

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

    beast_locker_rdlock(beast_cache_locker); /* read lock */

    for (i = 0; i < BUCKETS_DEFAULT_SIZE; i++) {
        item = beast_cache_buckets[i];
        while (item) {
            sprintf(key, "{device(%d)#inode(%d)}",
                  item->key.device, item->key.inode);
            add_assoc_long(retval, key, item->key.fsize);
            item = item->next;
        }
    }

    beast_locker_unlock(beast_cache_locker);
}


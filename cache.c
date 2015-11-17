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
#include "spinlock.h"
#include "php.h"
#include "cache.h"
#include "beast_log.h"


#define BUCKETS_DEFAULT_SIZE 1021


static int beast_cache_initialization = 0;
static cache_item_t **beast_cache_buckets = NULL;
static beast_atomic_t *cache_lock;


static inline int beast_cache_hash(cache_key_t *key)
{
    return key->device * 3 + key->inode * 7;
}


int beast_cache_init(int size)
{
    int index, bucket_size;

    if (beast_cache_initialization) {
        return 0;
    }

    if (beast_mm_init(size) == -1) {
        return -1;
    }

    /* init cache lock */
    cache_lock = (int *)mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE,
                                                MAP_SHARED|MAP_ANON, -1, 0);
    if (!cache_lock) {
        beast_write_log(beast_log_error,
                                    "Unable alloc share memory for cache lock");
        beast_mm_destroy();
        return -1;
    }

    *cache_lock = 0;

    /* init cache buckets's memory */
    bucket_size = sizeof(cache_item_t *) * BUCKETS_DEFAULT_SIZE;
    beast_cache_buckets = (cache_item_t **)mmap(NULL, bucket_size,
                              PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
    if (!beast_cache_buckets) {
        beast_write_log(beast_log_error,
                                 "Unable alloc share memory for cache buckets");
        munmap(cache_lock, sizeof(int));
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
    int pid = (int)getpid();

    beast_spinlock(cache_lock, pid);

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

    beast_spinunlock(cache_lock, pid);

    return item;
}


cache_item_t *beast_cache_create(cache_key_t *key, int size)
{
    cache_item_t *item, *next;
    int i, msize, bsize;
    int pid = (int)getpid();

    msize = sizeof(*item) + size;
    bsize = sizeof(cache_item_t *) * BUCKETS_DEFAULT_SIZE;

    if ((msize + bsize) > beast_mm_realspace()) {
        beast_write_log(beast_log_error, "Cache item size too big");
        return NULL;
    }

    item = beast_mm_malloc(msize);

    if (!item) {

#if 0
        int index;

        /* clean all caches */

        beast_spinlock(cache_lock, pid);

        for (index = 0; index < BUCKETS_DEFAULT_SIZE; index++) {
            beast_cache_buckets[index] = NULL;
        }

        beast_mm_flush();

        beast_spinunlock(cache_lock, pid);

        item = beast_mm_malloc(msize);
        if (!item) {
            return NULL;
        }
#endif

        beast_write_log(beast_log_notice, "Not enough caches, "
            "please setting <beast.cache_size> bigger in `php.ini' file");
        return NULL;
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
    int pid = (int)getpid();
    
    beast_spinlock(cache_lock, pid);

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
                beast_spinunlock(cache_lock, pid);
                return self;
            } else { /* do replace */
                item->next = self->next;
                beast_mm_free(self);
                *this = item;
                beast_spinunlock(cache_lock, pid);
                return item;
            }
        }
        this = &self->next;
    }

    *this = item;
#endif

    item->next = beast_cache_buckets[index];
    beast_cache_buckets[index] = item;

    beast_spinunlock(cache_lock, pid);
    
    return item;
}


int beast_cache_destroy()
{
    int index;
    cache_item_t *item, *next;
    int pid = (int)getpid();

    if (!beast_cache_initialization) {
        return 0;
    }

    beast_mm_destroy(); /* destroy memory manager */

    /* free cache buckets's mmap memory */
    munmap(beast_cache_buckets, sizeof(cache_item_t *) * BUCKETS_DEFAULT_SIZE);

    munmap(cache_lock, sizeof(int));

    beast_cache_initialization = 0;

    return 0;
}


void beast_cache_info(zval *retval)
{
    char key[128];
    int i;
    cache_item_t *item;
    int pid = (int)getpid();

    beast_spinlock(cache_lock, pid);

    for (i = 0; i < BUCKETS_DEFAULT_SIZE; i++) {
        item = beast_cache_buckets[i];
        while (item) {
            sprintf(key, "{device(%d)#inode(%d)}",
                  item->key.device, item->key.inode);
            add_assoc_long(retval, key, item->key.fsize);
            item = item->next;
        }
    }

    beast_spinunlock(cache_lock, pid);
}


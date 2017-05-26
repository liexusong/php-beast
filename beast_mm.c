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
  | Author: Liexusong <liexusong@qq.com>                                 |
  +----------------------------------------------------------------------+
*/

/*
 * The simple share memory manager algorithm
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef PHP_WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

#include "spinlock.h"
#include "beast_log.h"
#include "shm.h"

#define BEAST_SEGMENT_DEFAULT_SIZE (256 * 1024)

#define BLOCKAT(addr, offset)  ((beast_block_t *)((char *)(addr) + (offset)))

#ifdef max
#undef max
#endif
#define max(a, b) ((a) > (b) ? (a) : (b))

typedef struct beast_header_s {
    int segsize;    /* size of entire segment */
    int avail;      /* bytes available memorys */
} beast_header_t;

typedef struct beast_block_s {
    int size;       /* size of this block */
    int next;       /* offset in segment of next free block */
} beast_block_t;

static int beast_mm_initialized = 0;
static void *beast_mm_block = NULL;
static int beast_mm_block_size = 0;
static beast_atomic_t *mm_lock;
extern int beast_pid;

void beast_mm_lock()
{
    beast_spinlock(mm_lock, beast_pid);
}

void beast_mm_unlock()
{
    beast_spinunlock(mm_lock, beast_pid);
}

/*
 * memory align function
 * @param bits, align bits
 */
static inline int beast_mm_alignmem(int bits)
{
    typedef union {
        void* p;
        int i;
        long l;
        double d;
        void (*f)();
    } beast_word_t; /* may be 8 bits */

    return sizeof(beast_word_t) * (1 + ((bits - 1) / sizeof(beast_word_t)));
}

static int beast_mm_allocate(void *shmaddr, int size)
{
    beast_header_t *header;       /* header of shared memory segment */
    beast_block_t *prv;           /* block prior to working block */
    beast_block_t *cur;           /* working block in list */
    beast_block_t *prvbestfit;    /* block before best fit */
    int realsize;                 /* actual size of block needed, including header */
    int minsize;                  /* for finding best fit */
    int offset;

    /* Realsize must be aligned to a word boundary on some architectures. */
    realsize = size + beast_mm_alignmem(sizeof(int));
    realsize = beast_mm_alignmem(max(realsize, sizeof(beast_block_t)));

    /*
     * First, insure that the segment contains at least realsize free bytes,
     * even if they are not contiguous.
     */
    header = (beast_header_t *)shmaddr;
    if (header->avail < realsize) {
        beast_write_log(beast_log_error,
                        "Not enough memory for beast_mm_alloc()");
        return -1;
    }

    prvbestfit = 0;    /* Best block prev's node */
    minsize = INT_MAX;

    prv = BLOCKAT(shmaddr, sizeof(beast_header_t)); /* Free list header */

    while (prv->next != 0) {
        cur = BLOCKAT(shmaddr, prv->next); /* Current active block */
        if (cur->size == realsize) {
            prvbestfit = prv;
            break;
        }
        else if (cur->size > (sizeof(beast_block_t) + realsize)
                 && cur->size < minsize)
        {
            prvbestfit = prv;
            minsize = cur->size;
        }
        prv = cur;
    }

    if (prvbestfit == 0) { /* Not found best block */
        return -1;
    }

    prv = prvbestfit;
    cur = BLOCKAT(shmaddr, prv->next);

    /* update the block header */
    header->avail -= realsize;

    if (cur->size == realsize) {
        prv->next = cur->next;

    } else {
        beast_block_t *nxt;   /* The new block (chopped part of cur) */
        int nxtoffset;        /* Offset of the block currently after cur */
        int oldsize;          /* Size of cur before split */

        /* bestfit is too big; split it into two smaller blocks */
        nxtoffset = cur->next;
        oldsize = cur->size;
        prv->next += realsize;
        cur->size = realsize;
        nxt = BLOCKAT(shmaddr, prv->next);
        nxt->next = nxtoffset;
        nxt->size = oldsize - realsize;
    }

    /* skip size field */

    offset = (char *)cur - (char *)shmaddr;

    return offset + beast_mm_alignmem(sizeof(int));
}

static int beast_mm_deallocate(void *shmaddr, int offset)
{
    beast_header_t *header;   /* Header of shared memory segment */
    beast_block_t *cur;       /* The new block to insert */
    beast_block_t *prv;       /* The block before cur */
    beast_block_t *nxt;       /* The block after cur */
    int size;                 /* Size of deallocated block */

    offset -= beast_mm_alignmem(sizeof(int)); /* Really offset */

    /* Find position of new block in free list */
    prv = BLOCKAT(shmaddr, sizeof(beast_header_t));

    while (prv->next != 0 && prv->next < offset) {
        prv = BLOCKAT(shmaddr, prv->next);
    }

    /* Insert new block after prv */
    cur = BLOCKAT(shmaddr, offset);
    cur->next = prv->next;
    prv->next = offset;

    /* Update the block header */
    header = (beast_header_t *)shmaddr;
    header->avail += cur->size;
    size = cur->size;

    if (((char *)prv) + prv->size == (char *) cur) {
        /* cur and prv share an edge, combine them */
        prv->size += cur->size;
        prv->next = cur->next;
        cur = prv;
    }

    nxt = BLOCKAT(shmaddr, cur->next);
    if (((char *)cur) + cur->size == (char *) nxt) {
        /* cur and nxt shared an edge, combine them */
        cur->size += nxt->size;
        cur->next = nxt->next;
    }

    return size;
}

void beast_mm_reinit()
{
    beast_header_t *header;
    beast_block_t  *block;

    header = (beast_header_t *)beast_mm_block;
    header->segsize = beast_mm_block_size;
    header->avail = beast_mm_block_size
                    - sizeof(beast_header_t)
                    - sizeof(beast_block_t)
                    - beast_mm_alignmem(sizeof(int));

    /* The free list head block node */
    block = BLOCKAT(beast_mm_block, sizeof(beast_header_t));
    block->size = 0;
    block->next = sizeof(beast_header_t) + sizeof(beast_block_t);

    /* The avail block */
    block = BLOCKAT(beast_mm_block, block->next);
    block->size = header->avail;
    block->next = 0;
}

/*
 * Init memory manager
 */
int beast_mm_init(int block_size)
{
    if (beast_mm_initialized) {
        return 0;
    }

    /* Init memory manager lock */
    mm_lock = (int *)beast_shm_alloc(sizeof(beast_atomic_t));
    if (!mm_lock) {
        beast_write_log(beast_log_error,
                        "Unable alloc share memory for memory manager lock");
        return -1;
    }

    *mm_lock = 0;

    /* Init share memory for beast */
    if (block_size < BEAST_SEGMENT_DEFAULT_SIZE) {
        beast_mm_block_size = BEAST_SEGMENT_DEFAULT_SIZE;
    } else {
        beast_mm_block_size = block_size;
    }

    beast_mm_block = (void *)beast_shm_alloc(beast_mm_block_size);
    if (!beast_mm_block) {
        beast_write_log(beast_log_error,
                        "Unable alloc share memory for beast");
        beast_shm_free((void *)mm_lock, sizeof(beast_atomic_t));
        return -1;
    }

    beast_mm_reinit();

    beast_mm_initialized = 1;

    return 0;
}

void *beast_mm_malloc(int size)
{
    int offset;
    void *p = NULL;

    beast_mm_lock();

    offset = beast_mm_allocate(beast_mm_block, size);
    if (offset != -1) {
        p = (void *)(((char *)beast_mm_block) + offset);
    }

    beast_mm_unlock();

    return p;
}

void *beast_mm_calloc(int size)
{
    int offset;
    void *p = NULL;

    beast_mm_lock();

    offset = beast_mm_allocate(beast_mm_block, size);
    if (offset != -1) {
        p = (void *)(((char *)beast_mm_block) + offset);
    }

    beast_mm_unlock();

    if (NULL != p) {
        memset(p, 0, size);
    }

    return p;
}

void beast_mm_free(void *p)
{
    int offset;

    offset = (unsigned int)((char *)p - (char *)beast_mm_block);
    if (offset <= 0) {
        return;
    }

    beast_mm_lock();
    beast_mm_deallocate(beast_mm_block, offset);
    beast_mm_unlock();
}

void beast_mm_flush()
{
    beast_mm_lock();
    beast_mm_reinit();
    beast_mm_unlock();
}

/*
 * Get the avail's memory space
 */
int beast_mm_availspace()
{
    int size;
    beast_header_t *header = (beast_header_t *)beast_mm_block;

    beast_mm_lock();
    size = header->avail;
    beast_mm_unlock();

    return size;
}

/*
 * Don't locked here, because the segsize not change forever
 */
int beast_mm_realspace()
{
    int size;

    beast_mm_lock();
    size = ((beast_header_t *)beast_mm_block)->segsize;
    beast_mm_unlock();

    return size;
}

/*
 * Destroy memory's manager
 */
void beast_mm_destroy()
{
    if (beast_mm_initialized) {
        beast_shm_free((void *)beast_mm_block, beast_mm_block_size);
        beast_shm_free((void *)mm_lock, sizeof(beast_atomic_t));
        beast_mm_initialized = 0;
    }
}

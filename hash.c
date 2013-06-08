/*
 * Copyright (c) 2011, Liexusong <liexusong@qq.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include "hash.h"

static unsigned int tableSize[] = {
    7,          13,         31,         61,         127,        251,
    509,        1021,       2039,       4093,       8191,       16381,
    32749,      65521,      131071,     262143,     524287,     1048575,
    2097151,    4194303,    8388607,    16777211,   33554431,   67108863,
    134217727,  268435455,  536870911,  1073741823, 2147483647, 0
};

static void hash_resize(HashTable *old);

#define mix(a,b,c)                \
{                                 \
  a -= b; a -= c; a ^= (c>>13);   \
  b -= c; b -= a; b ^= (a<<8);    \
  c -= a; c -= b; c ^= (b>>13);   \
  a -= b; a -= c; a ^= (c>>12);   \
  b -= c; b -= a; b ^= (a<<16);   \
  c -= a; c -= b; c ^= (b>>5);    \
  a -= b; a -= c; a ^= (c>>3);    \
  b -= c; b -= a; b ^= (a<<10);   \
  c -= a; c -= b; c ^= (b>>15);   \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  len     : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

ub4 hash( k, length, initval )
     register ub1 *k;        /* the key */
     register ub4  length;   /* the length of the key */
     register ub4  initval;  /* the previous hash, or an arbitrary value */
{
    register ub4 a,b,c,len;

    /* Set up the internal state */
    len = length;
    a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
    c = initval;         /* the previous hash value */

    /*---------------------------------------- handle most of the key */
    while (len >= 12)
        {
            a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
            b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
            c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
            mix(a,b,c);
            k += 12; len -= 12;
        }

    /*------------------------------------- handle the last 11 bytes */
    c += length;
    switch(len)              /* all the case statements fall through */
        {
        case 11: c+=((ub4)k[10]<<24);
        case 10: c+=((ub4)k[9]<<16);
        case 9 : c+=((ub4)k[8]<<8);
            /* the first byte of c is reserved for the length */
        case 8 : b+=((ub4)k[7]<<24);
        case 7 : b+=((ub4)k[6]<<16);
        case 6 : b+=((ub4)k[5]<<8);
        case 5 : b+=k[4];
        case 4 : a+=((ub4)k[3]<<24);
        case 3 : a+=((ub4)k[2]<<16);
        case 2 : a+=((ub4)k[1]<<8);
        case 1 : a+=k[0];
            /* case 0: nothing left to add */
        }
    mix(a,b,c);
    /*-------------------------------------------- report the result */
    return c;
}

HashTable *hash_alloc(int size) {
    HashTable *htb;
    
    htb = (HashTable *)malloc(sizeof(HashTable));
    if (!htb) {
        return NULL;
    }
    
    htb->size = htb->used = 0;
    
    while (tableSize[htb->size] < size) {
        htb->size++;
        if (tableSize[htb->size] == 0) {
            htb->size--;
            break;
        }
    }
    
    htb->bucket = (HashNode **)malloc(tableSize[htb->size] * sizeof(HashNode *));
    if (!htb->bucket) {
        free(htb);
        return NULL;
    }
    
    memset(htb->bucket, 0, tableSize[htb->size] * sizeof(HashNode *));
    
    INIT_LIST_HEAD(&htb->list);
    
    return htb;
}


int hash_insert(HashTable *htb, char *key, void *value) {
    int slen;
    ub4 h, index;
    HashNode *node;
    
    slen = strlen(key);
    
    node = (HashNode *)calloc(1, sizeof(HashNode) + slen + 1);
    if (!node) {
        return -1;
    }
    
    h = hash(key, slen, 0);
    index = h % tableSize[htb->size];

    node->h = h;
    node->value = value;
    node->keyLength = slen;
    memcpy(node->key, key, node->keyLength);
    node->key[node->keyLength] = '\0';
    
    node->next = htb->bucket[index];
    htb->bucket[index] = node;
    
    list_add_tail(&node->list, &htb->list);
    
    htb->used++;
    
    hash_try_resize(htb);
    
    return 0;
}

int hash_insert_bykey(HashTable *htb, HashKey *key, void *value) {
    ub4 h, index;
    HashNode *node;
    
    node = (HashNode *)calloc(1, sizeof(HashNode) + key->keyLength + 1);
    if (!node) {
        return -1;
    }
    
    h = hash(key, key->keyLength, 0);
    index = h % tableSize[htb->size];

    node->h = h;
    node->value = value;
    node->keyLength = key->keyLength;
    memcpy(node->key, key->key, key->keyLength);
    node->key[key->keyLength] = '\0';
    
    node->next = htb->bucket[index];
    htb->bucket[index] = node;
    
    list_add_tail(&node->list, &htb->list);
    
    htb->used++;
    
    hash_try_resize(htb);
    
    return 0;
}

int hash_lookup(HashTable *htb, char *key, void **retval) {
    ub4 index;
    HashNode *node;
    
    index = hash(key, strlen(key), 0) % tableSize[htb->size];
    
    node = htb->bucket[index];
    while (node && strncmp(node->key, key, node->keyLength)) {
        node = node->next;
    }
    
    if (!node) {
        *retval = (void *)NULL;
        return -1;
    } else {
        *retval = node->value;
        return 0;
    }
}

int hash_replace(HashTable *htb, char *key, void *nvalue, void **retval) {
    ub4 index;
    HashNode *node;
    
    index = hash(key, strlen(key), 0) % tableSize[htb->size];
    
    node = htb->bucket[index];
    while (node && strncmp(node->key, key, node->keyLength)) {
        node = node->next;
    }
    
    if (!node) {
        *retval = (void *)NULL;
        return -1;
    } else {
        *retval = node->value;
        node->value = nvalue;
        return 0;
    }
}


int hash_remove(HashTable *htb, char *key, void **retval) {
    ub4 index;
    HashNode *node, *prev;
    
    index = hash(key, strlen(key), 0) % tableSize[htb->size];
    
    prev = NULL;
    node = htb->bucket[index];
    while (node && strncmp(node->key, key, node->keyLength)) {
        prev = node;
        node = node->next;
    }
    
    if (!node) {
        *retval = (void *)NULL;
        return -1;
    }
    
    list_del(&node->list);
    
    *retval = node->value;
    if (!prev) {
        htb->bucket[index] = node->next;
    } else {
        prev->next = node->next;
    }
    free(node);
    
    htb->used--;
    return 0;
}

void hash_destroy(HashTable *htb, hash_destroy_function destroy) {
    HashNode *node, *next;
    int i;
    
    for (i = 0; i < tableSize[htb->size]; i++) {
        node = htb->bucket[i];
        while (node) {
            next = node->next;
            destroy(node->value);
            free(node);
            node = next;
        }
    }
    free(htb);
    
    return;
}

void hash_try_resize(HashTable *htb) {
    int limit = tableSize[htb->size] * 0.8;
    
    if (htb->used >= limit) {
        hash_resize(htb);
    }
    return;
}

static void hash_resize(HashTable *h_old) {
    HashTable ht;
    int i;

    ht.used = h_old->used;
    ht.size = h_old->size;
    
    while (tableSize[ht.size] && tableSize[ht.size] <= ht.used) ht.size++;
    
    if (tableSize[ht.size] == 0) return;
    
    ht.bucket = (HashNode **)malloc(sizeof(HashNode *) * tableSize[ht.size]);
    if (ht.bucket == NULL) {
        return;
    }
    
    memset(ht.bucket, 0, sizeof(HashNode *) * tableSize[ht.size]);

    for (i = 0; i < tableSize[h_old->size]; i++) {
        HashNode *e = h_old->bucket[i];
        HashNode *next_e;
        
        while (e) {
            unsigned int index = e->h % tableSize[ht.size];
            
            next_e = e->next;
            
            e->next = ht.bucket[index];
            ht.bucket[index] = e;
            
            e = next_e;
        }
    }
    free(h_old->bucket);

    h_old->bucket = ht.bucket;
    h_old->size = ht.size;
    
    return;
}


int hash_foreach(HashTable *htb, hash_foreach_handler handler) {
    struct list_head *position, *temp;
    HashNode *node;
    
    list_for_each_safe(position, temp, &htb->list) {
        node = list_entry(position, HashNode, list);
        if (handler(node->key, node->keyLength, node->value) == -1) {
            return -1;
        }
    }
    return 0;
}


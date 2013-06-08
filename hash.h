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

#ifndef HASH_H
#define HASH_H

#include "list.h"

typedef unsigned long int ub4;
typedef unsigned char ub1;

typedef struct sHashNode  HashNode;
typedef struct sHashTable HashTable;
typedef struct sHashKey   HashKey;

struct sHashNode {
	struct list_head list;
	HashNode	*next;
	void		*value;
	ub4			h;
	int			keyLength;
	char		key[1];
};

struct sHashTable {
	unsigned int size;
	unsigned int used;
	HashNode **bucket;
	struct list_head list;
};

struct sHashKey {
	char *key;
	int keyLength;
};

typedef void (*hash_destroy_function)(void *);
typedef int (*hash_foreach_handler)(char *key, int keyLength, void *value);

HashTable *hash_alloc(int size);
int hash_insert(HashTable *htb, char *key, void *value);
int hash_insert_bykey(HashTable *htb, HashKey *key, void *value);
int hash_lookup(HashTable *htb, char *key, void **retval);
int hash_replace(HashTable *htb, char *key, void *nvalue, void **retval);
int hash_remove(HashTable *htb, char *key, void **retval);
void hash_destroy(HashTable *htb, hash_destroy_function destroy);
void hash_try_resize(HashTable *htb);
int hash_foreach(HashTable *htb, hash_foreach_handler handler);

#endif

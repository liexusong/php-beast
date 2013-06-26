#ifndef __BEAST_CACHE_H
#define __BEAST_CACHE_H

typedef struct cache_key_s {
	int device;
	int inode;
	int mtime;
	int fsize;
} cache_key_t;


typedef struct cache_item_s {
	cache_key_t key;
	struct cache_item_s *next;
	char data[0];
} cache_item_t;


#define beast_cache_cdata(item)  (item)->data
#define beast_cache_fsize(item)  (item)->key.fsize

int beast_cache_init();
cache_item_t *beast_cache_find(cache_key_t *key);
cache_item_t *beast_cache_create(cache_key_t *key, int size);
cache_item_t *beast_cache_push(cache_item_t *item);
int beast_cache_destroy();

#endif

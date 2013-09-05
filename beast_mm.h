#ifndef __BEAST_MM_H
#define __BEAST_MM_H

int beast_mm_init(int block_size);
void *beast_mm_malloc(int size);
void *beast_mm_calloc(int size);
int beast_mm_free(void *p);
int beast_mm_flush();
int beast_mm_availspace();
int beast_mm_realspace();
void beast_mm_destroy();

#endif

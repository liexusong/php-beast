#ifndef __SHM_H
#define __SHM_H

#include <stddef.h>

void *beast_shm_alloc(size_t size);
int beast_shm_free(void *p, size_t size);

#endif

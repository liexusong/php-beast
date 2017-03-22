#include "shm.h"

#ifdef PHP_WIN32
    #include <Windows.h>
#else
    #include <sys/mman.h>

    #ifndef MAP_NOSYNC
        #define MAP_NOSYNC 0
    #endif
#endif

void *beast_shm_alloc(size_t size)
{
    void *p;
#ifdef PHP_WIN32
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE,
		NULL, PAGE_READWRITE, 0, size, NULL);

	if (hMapFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    p = MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            size);
    CloseHandle(hMapFile);

#else

    p = mmap(NULL,
            size,
            PROT_READ|PROT_WRITE,
            MAP_SHARED|MAP_ANON,
            -1,
            0);

#endif
    return p;
}

int beast_shm_free(void *p, size_t size)
{
#ifdef PHP_WIN32
	return UnmapViewOfFile(p) ? 0 : -1;
#else
    return munmap(p, size);
#endif
}


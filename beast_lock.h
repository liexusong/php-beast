#ifndef __BEAST_LOCK_H
#define __BEAST_LOCK_H

typedef struct {
    int fd;
    char *path;
} beast_locker_t;

extern char *beast_lock_path;

beast_locker_t *beast_locker_create(char *path);
void beast_locker_wrlock(beast_locker_t *locker);
void beast_locker_rdlock(beast_locker_t *locker);
void beast_locker_unlock(beast_locker_t *locker);
void beast_locker_destroy(beast_locker_t *locker);

#define beast_locker_lock(locker)  beast_locker_wrlock(locker)

#endif

#ifndef __BEAST_LOCK_H
#define __BEAST_LOCK_H

int beast_sem_create(int initval);
int beast_sem_lock(int semid);
int beast_sem_unlock(int semid);
int beast_sem_destroy(int semid);

typedef int beast_locker_t;

#define beast_locker_create()         beast_sem_create(1)
#define beast_locker_lock(locker)     beast_sem_lock(locker)
#define beast_locker_unlock(locker)   beast_sem_unlock(locker)
#define beast_locker_destroy(locker)  beast_sem_destroy(locker)

#endif

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifndef HAVE_SEMUN
union semun {
    int val;                  /* value for SETVAL */
    struct semid_ds *buf;     /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array;    /* array for GETALL, SETALL */
                              /* Linux specific part: */
    struct seminfo *__buf;    /* buffer for IPC_INFO */
};
#endif

#ifndef SEM_R
# define SEM_R 0444
#endif
#ifndef SEM_A
# define SEM_A 0222
#endif

/* always use SEM_UNDO, otherwise we risk deadlock */
#define USE_SEM_UNDO

#ifdef USE_SEM_UNDO
# define UNDO SEM_UNDO
#else
# define UNDO 0
#endif

int beast_sem_create(int initval)
{
	int semid;
    union semun arg;
    key_t key = IPC_PRIVATE;

    if ((semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0777)) >= 0) {
        arg.val = initval;
        if (semctl(semid, 0, SETVAL, arg) < 0) {
            return -1;
        }
    } else if (errno == EEXIST) {
        if ((semid = semget(key, 1, 0777)) < 0) {
            return -1;
        }
    } else {
        return -1;
    }
    return semid;
}

int beast_sem_lock(int semid)
{
	struct sembuf op;

    op.sem_num = 0;
    op.sem_op  = -1;   /* we want alloc 1 sem */
    op.sem_flg = UNDO;

    if (semop(semid, &op, 1) < 0) {
        return -1;
    }
    return 0;
}

int beast_sem_unlock(int semid)
{
	struct sembuf op;

    op.sem_num = 0;
    op.sem_op  = 1;
    op.sem_flg = UNDO;

    if (semop(semid, &op, 1) < 0) {
        return -1;
    }
    return 0;
}

int beast_sem_destroy(int semid)
{
	union semun arg;
    semctl(semid, 0, IPC_RMID, arg);
}

#ifndef __BEAST_SPINLOCK_H
#define __BEAST_SPINLOCK_H

typedef volatile unsigned int beast_atomic_t;

void beast_spinlock(beast_atomic_t *lock, int pid);
void beast_spinunlock(beast_atomic_t *lock, int pid);

#endif

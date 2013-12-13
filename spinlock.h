#ifndef __BEAST_SPINLOCK_H
#define __BEAST_SPINLOCK_H

void beast_spinlock(int *lock, int pid);
void beast_spinunlock(int *lock, int pid);

#endif

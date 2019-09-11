/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Liexusong <280259971@qq.com>                                 |
  +----------------------------------------------------------------------+
*/

#include <stdlib.h>
#include "spinlock.h"
#ifdef PHP_WIN32
  #include <Windows.h>
#else
  #include <pthread.h>
#endif
#include "beast_log.h"

#ifdef PHP_WIN32
  #define compare_and_swap(lock, o, n) \
      (InterlockedCompareExchange(lock, n, o) == n)
  #define pause() YieldProcessor()
  #define yield() SwitchToThread()
#else
  #define compare_and_swap(lock, o, n) \
      __sync_bool_compare_and_swap(lock, o, n)
  #ifdef __arm__
    #define pause() __asm__("NOP");
  #else
    #define pause() __asm__("pause")
  #endif
  #define yield() sched_yield()
#endif

extern int beast_ncpu;

void beast_spinlock(beast_atomic_t *lock, int pid)
{
    int i, n;

    for ( ;; ) {
        if (compare_and_swap(lock, 0, pid)) {
            return;
        }

        if (beast_ncpu > 1) {

            for (n = 1; n < 129; n <<= 1) {

                if (compare_and_swap(lock, 0, pid)) {
                    return;
                }

                for (i = 0; i < n; i++) {
                    pause();
                }
            }
        }

        yield();
    }
}

void beast_spinunlock(beast_atomic_t *lock, int pid)
{
    compare_and_swap(lock, pid, 0);
}

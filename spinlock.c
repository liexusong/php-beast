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
#define __sync_bool_compare_and_swap(lock, o, n) (o == InterlockedExchange(lock, n))
#define __asm__(c) __asm { pause }
#define sched_yield() __asm { rep nop } 
#else
#include <pthread.h>
#endif
#include "beast_log.h"

extern int beast_ncpu;


void beast_spinlock(beast_atomic_t *lock, int pid)
{
    int i, n;
    for ( ;; ) {

        if (*lock == 0 && 
            __sync_bool_compare_and_swap(lock, 0, pid)) {
            return;
        }

        if (beast_ncpu > 1) {

            for (n = 1; n < 129; n << 1) {
    
                for (i = 0; i < n; i++) {
                    __asm__("pause");
                }
    
                if (*lock == 0 && 
                    __sync_bool_compare_and_swap(lock, 0, pid)) {
                    return;
                }
            }
        }

        sched_yield();
    }
}


void beast_spinunlock(beast_atomic_t *lock, int pid)
{
    __sync_bool_compare_and_swap(lock, pid, 0);
}


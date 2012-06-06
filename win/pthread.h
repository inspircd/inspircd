/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 * POSIX emulation layer for Windows.
 *
 *   Copyright (C) 2012 Anope Team <team@anope.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <Windows.h>

typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef HANDLE pthread_cond_t;
typedef int pthread_attr_t;
typedef void pthread_mutexattr_t;
typedef void pthread_condattr_t;

#define PTHREAD_CREATE_JOINABLE 0

extern int pthread_attr_init(pthread_attr_t *);
extern int pthread_attr_setdetachstate(pthread_attr_t *, int);
extern int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
extern int pthread_join(pthread_t, void **);
extern void pthread_exit(int);

extern int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
extern int pthread_mutex_destroy(pthread_mutex_t *);
extern int pthread_mutex_lock(pthread_mutex_t *);
extern int pthread_mutex_trylock(pthread_mutex_t *);
extern int pthread_mutex_unlock(pthread_mutex_t *);

extern int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
extern int pthread_cond_destroy(pthread_cond_t *);
extern int pthread_cond_signal(pthread_cond_t *);
extern int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);

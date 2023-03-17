/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "threadengine.h"
#include "anope.h"

#ifndef _WIN32
#include <pthread.h>
#endif

static inline pthread_attr_t *get_engine_attr() {
    /* Threadengine attributes used by this thread engine */
    static pthread_attr_t attr;
    static bool inited = false;

    if (inited == false) {
        if (pthread_attr_init(&attr)) {
            throw CoreException("Error calling pthread_attr_init");
        }
        if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) {
            throw CoreException("Unable to mark threads as joinable");
        }
        inited = true;
    }

    return &attr;
}

static void *entry_point(void *parameter) {
    Thread *thread = static_cast<Thread *>(parameter);
    thread->Run();
    thread->SetExitState();
    pthread_exit(0);
    return NULL;
}

Thread::Thread() : exit(false) {
}

Thread::~Thread() {
}

void Thread::Join() {
    this->SetExitState();
    pthread_join(handle, NULL);
}

void Thread::SetExitState() {
    this->Notify();
    exit = true;
}

void Thread::Exit() {
    this->SetExitState();
    pthread_exit(0);
}

void Thread::Start() {
    if (pthread_create(&this->handle, get_engine_attr(), entry_point, this)) {
        this->flags[SF_DEAD] = true;
        throw CoreException("Unable to create thread: " + Anope::LastError());
    }
}

bool Thread::GetExitState() const {
    return exit;
}

void Thread::OnNotify() {
    this->Join();
    this->flags[SF_DEAD] = true;
}

Mutex::Mutex() {
    pthread_mutex_init(&mutex, NULL);
}

Mutex::~Mutex() {
    pthread_mutex_destroy(&mutex);
}

void Mutex::Lock() {
    pthread_mutex_lock(&mutex);
}

void Mutex::Unlock() {
    pthread_mutex_unlock(&mutex);
}

bool Mutex::TryLock() {
    return pthread_mutex_trylock(&mutex) == 0;
}

Condition::Condition() : Mutex() {
    pthread_cond_init(&cond, NULL);
}

Condition::~Condition() {
    pthread_cond_destroy(&cond);
}

void Condition::Wakeup() {
    pthread_cond_signal(&cond);
}

void Condition::Wait() {
    pthread_cond_wait(&cond, &mutex);
}

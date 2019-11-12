// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include "tprintf.h"
#include <map>

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu)
{
    srand(time(NULL) ^ last_port);
    rlock_port = ((rand() % 32000) | (0x1 << 10));
    const char *hname;
    // VERIFY(gethostname(hname, 100) == 0);
    hname = "127.0.0.1";
    std::ostringstream host;
    host << hname << ":" << rlock_port;
    id = host.str();
    last_port = rlock_port;
    rpcs *rlsrpc = new rpcs(rlock_port);
    rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
    rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
    pthread_mutex_init(&lockLock, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
    pthread_mutex_lock(&lockLock);
    lock_entry *lock;
    lock_protocol::status ret = lock_protocol::OK;
    std::map<lock_protocol::lockid_t, lock_entry *>::iterator iter = locks.find(lid);
    if (iter == locks.end())
    {
        lock = new lock_entry();
        lock->state = NONE;
        lock->message = EMPTY;
        locks.insert(std::pair<lock_protocol::lockid_t, lock_entry *>(lid, lock));
    }
    lock = locks.find(lid)->second;

    thread_cond *thread = new thread_cond();
    pthread_cond_init(&(thread->cond), NULL);

    lock->waiting_threads.push(thread);
    bool ifempty = lock->waiting_threads.size() <= 1;
    if (!ifempty)
    {
        pthread_cond_wait(&thread->cond, &lockLock);
    }

    ret = acquiring(lock, lid, ifempty);
    return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
    int r = 0;
    lock_protocol::status ret = rlock_protocol::OK;
    pthread_mutex_lock(&lockLock);
    lock_entry *lock = locks.find(lid)->second;

    free(lock->waiting_threads.front());
    lock->waiting_threads.pop();

    if (lock->message == REVOKE)
    {
        lock->state = RELEASING;
        pthread_mutex_unlock(&lockLock);
        ret = cl->call(lock_protocol::release, lid, id, r);
        pthread_mutex_lock(&lockLock);
        lock->message = EMPTY;
        lock->state = NONE;

        if (!lock->waiting_threads.empty())
        {
            thread_cond *thread = lock->waiting_threads.front();
            pthread_cond_signal(&thread->cond);
        }
    }
    else
    {
        lock->state = FREE;
        if (!lock->waiting_threads.empty())
        {
            lock->state = LOCKED;
            thread_cond *thread = lock->waiting_threads.front();
            pthread_cond_signal(&thread->cond);
        }
    }
    pthread_mutex_unlock(&lockLock);
    return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &)
{
    int r = 0;
    int ret = rlock_protocol::OK;
    pthread_mutex_lock(&lockLock);
    lock_entry *lock = locks.find(lid)->second;

    if (lock->state == FREE)
    {
        lock->state = RELEASING;
        pthread_mutex_unlock(&lockLock);
        ret = cl->call(lock_protocol::release, lid, id, r);
        pthread_mutex_lock(&lockLock);
        lock->state = NONE;
        if (!lock->waiting_threads.empty())
        {
            thread_cond *thread = lock->waiting_threads.front();
            pthread_cond_signal(&thread->cond);
        }
    }
    else
    {
        lock->message = REVOKE;
    }

    pthread_mutex_unlock(&lockLock);
    return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int &)
{
    int ret = rlock_protocol::OK;
    pthread_mutex_lock(&lockLock);
    lock_entry *lock = locks.find(lid)->second;
    lock->message = RETRY;
    thread_cond *thread = lock->waiting_threads.front();
    pthread_cond_signal(&thread->cond);
    pthread_mutex_unlock(&lockLock);
    return ret;
}

lock_protocol::status lock_client_cache::acquiring(lock_entry *lock, lock_protocol::lockid_t lid, bool ifempty)
{
    int r = 0;
    lock_protocol::status ret = lock_protocol::OK;
    thread_cond *thread = lock->waiting_threads.front();
    switch (lock->state)
    {
    case NONE:
        lock->state = ACQUIRING;
        while (lock->state == ACQUIRING)
        {
            pthread_mutex_unlock(&lockLock);
            ret = cl->call(lock_protocol::acquire, lid, id, r);
            pthread_mutex_lock(&lockLock);
            if (ret == lock_protocol::OK)
            {
                lock->state = LOCKED;
                pthread_mutex_unlock(&lockLock);
                return lock_protocol::OK;
            }
            else
            {
                if (lock->message == EMPTY)
                {
                    pthread_cond_wait(&thread->cond, &lockLock);
                    lock->message = EMPTY;
                }
                else
                {
                    lock->message = EMPTY;
                }
            }
        }
        break;
    case FREE:
        lock->state = LOCKED;
        pthread_mutex_unlock(&lockLock);
        return lock_protocol::OK;
    case ACQUIRING:
    case RELEASING:
        if (ifempty)
        {
            pthread_cond_wait(&thread->cond, &lockLock);
            lock->state = ACQUIRING;
            while (lock->state == ACQUIRING)
            {
                pthread_mutex_unlock(&lockLock);
                ret = cl->call(lock_protocol::acquire, lid, id, r);
                pthread_mutex_lock(&lockLock);
                if (ret == lock_protocol::OK)
                {
                    lock->state = LOCKED;
                    pthread_mutex_unlock(&lockLock);
                    return lock_protocol::OK;
                }
                else
                {
                    if (lock->message == EMPTY)
                    {
                        pthread_cond_wait(&thread->cond, &lockLock);
                        lock->message = EMPTY;
                    }
                    else
                    {
                        lock->message = EMPTY;
                    }
                }
            }
        }
    default:
        break;
    }

    pthread_mutex_unlock(&lockLock);
    return lock_protocol::OK;
}

lock_client_cache::~lock_client_cache()
{
    pthread_mutex_lock(&lockLock);
    std::map<lock_protocol::lockid_t, lock_entry *>::iterator iter = locks.begin();
    while (iter != locks.end())
    {
        free(iter->second);
        iter++;
    }
    pthread_mutex_unlock(&lockLock);
}
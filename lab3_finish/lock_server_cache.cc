// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"
#include <map>

lock_server_cache::lock_server_cache()
{
    pthread_mutex_init(&lockLock, NULL);
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    pthread_mutex_lock(&lockLock);
    lock_entry *lock;
    std::map<lock_protocol::lockid_t, lock_entry *>::iterator iter = locks.find(lid);
    if (iter == locks.end())
    {
        lock = new lock_entry();
        lock->state = NONE;
        locks.insert(std::pair<lock_protocol::lockid_t, lock_entry *>(lid, lock));
    }

    lock = locks.find(lid)->second;
    switch (lock->state)
    {
    case NONE:
        lock->state = LOCKED;
        lock->userhost = id;
        pthread_mutex_unlock(&lockLock);
        return lock_protocol::OK;
    case LOCKED:
        lock->state = REVOKING;
        lock->waitinghosts.push(id);
        pthread_mutex_unlock(&lockLock);
        handle(lock->userhost).safebind()->call(rlock_protocol::revoke, lid, r);
        return lock_protocol::RETRY;
    case REVOKING:
        lock->waitinghosts.push(id);
        pthread_mutex_unlock(&lockLock);
        return lock_protocol::RETRY;
    case RETRYING:
        if (id != lock->retrying_host)
        {
            lock->waitinghosts.push(id);
            pthread_mutex_unlock(&lockLock);
            return lock_protocol::RETRY;
        }
        else
        {
            lock->state = LOCKED;
            lock->userhost = id;
            lock->retrying_host = "";
            if (!lock->waitinghosts.empty())
            {
                lock->state = REVOKING;
                pthread_mutex_unlock(&lockLock);
                handle(lock->userhost).safebind()->call(rlock_protocol::revoke, lid, r);
                return lock_protocol::OK;
            }
            pthread_mutex_unlock(&lockLock);
            return lock_protocol::OK;
        }        
    default:
        break;
    }
    return ret;
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
    pthread_mutex_lock(&lockLock);
    std::map<lock_protocol::lockid_t, lock_entry *>::iterator iter = locks.find(lid);
    lock_entry *lockentry = iter->second;
    std::string nexthost = lockentry->waitinghosts.front();
    lockentry->waitinghosts.pop();
    lockentry->retrying_host = nexthost;
    lockentry->state = RETRYING;
    lockentry->userhost = "";
    pthread_mutex_unlock(&lockLock);
    handle(nexthost).safebind()->call(rlock_protocol::retry, lid, r);
    return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
    tprintf("stat request\n");
    r = nacquire;
    return lock_protocol::OK;
}

lock_server_cache::~lock_server_cache()
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
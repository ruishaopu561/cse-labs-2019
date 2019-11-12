#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <queue>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_server_cache
{
private:
    int nacquire;
    enum STATE
    {
        NONE,
        LOCKED,
        REVOKING,
        RETRYING
    };

    struct lock_entry
    {
        std::string userhost;
        STATE state;
        std::string retrying_host;
        std::queue<std::string> waitinghosts;
    };

    std::map<lock_protocol::lockid_t, lock_entry *> locks;
    pthread_mutex_t lockLock;

public:
    lock_server_cache();
    ~lock_server_cache();
    lock_protocol::status stat(lock_protocol::lockid_t, int &);
    int acquire(lock_protocol::lockid_t, std::string id, int &);
    int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif

// extent client interface.

#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <string>
#include "extent_protocol.h"
#include "extent_server_cache.h"
#include <map>

class extent_client_cache
{
private:
    rpcc *cl;

    struct buf_entry
    {
        int buf_put_time;
        std::string buf;

        buf_entry()
        {
            buf_put_time = 0;
            buf = "";
        }
    };

    struct attr_entry
    {
        extent_protocol::attr attr;
    };

    std::map<extent_protocol::extentid_t, buf_entry *> files;
    std::map<extent_protocol::extentid_t, attr_entry *> attrs;

public:
    extent_client_cache(std::string dst);

    extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
    extent_protocol::status get(extent_protocol::extentid_t eid, std::string &buf);
    extent_protocol::status getattr(extent_protocol::extentid_t eid, extent_protocol::attr &a);
    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
    extent_protocol::status remove(extent_protocol::extentid_t eid);
};

#endif

// this is the extent server cache

#ifndef extent_server_cache_h
#define extent_server_cache_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server_cache
{
protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
    inode_manager *im;

    struct file_entry
    {
        int buf_put_time;
        std::string buf;
        int stale_file;

        file_entry()
        {
            buf_put_time = 0;
            buf = "";
            stale_file = 0;
        }
    };

    struct attr_entry
    {
        extent_protocol::attr attr;
    };
    
    std::map<extent_protocol::extentid_t, file_entry *> files;
    std::map<extent_protocol::extentid_t, attr_entry *> attrs;

public:
    extent_server_cache();

    int create(uint32_t type, extent_protocol::extentid_t &id);
    int put(extent_protocol::extentid_t id, std::string, int &);
    int get(extent_protocol::extentid_t id, std::string &);
    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
    int remove(extent_protocol::extentid_t id, int &);
};

#endif
// the extent server implementation

#include "extent_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>

extent_server_cache::extent_server_cache()
{
    im = new inode_manager();
}

int extent_server_cache::create(uint32_t type, extent_protocol::extentid_t &id)
{
    // alloc a new inode and return inum
    printf("extent_server_cache: create inode\n");
    std::map<extent_protocol::extentid_t, file_entry *>::iterator iter = files.find(id);
    if(iter == files.end())
    {
        id = im->alloc_inode(type);
        file_entry *file = new file_entry();
        files.insert(std::pair<extent_protocol::extentid_t, file_entry *>(id, file));
    }
    else
    {
        if(iter->second->stale_file == 1){
            iter->second->stale_file = 0;
            iter->second->buf = "";
            iter->second->buf_put_time = 0;
        }
        else{
            return extent_protocol::IOERR;
        }
    }
    
    return extent_protocol::OK;
}

int extent_server_cache::put(extent_protocol::extentid_t id, std::string buf, int &)
{
    id &= 0x7fffffff;

    printf("extent_server_cache: put %lld\n", id);

    std::map<extent_protocol::extentid_t, file_entry *>::iterator iter = files.find(id);
    if(iter == files.end())
    {
        file_entry *file = new file_entry();
        file->buf = buf;
        file->buf_put_time = 1;
        files.insert(std::pair<extent_protocol::extentid_t, file_entry *>(id, file));
        return extent_protocol::OK;
    }
    else if (iter->second->stale_file == 0)
    {
        iter->second->buf = buf;
        iter->second->buf_put_time++;
        if(iter->second->buf_put_time >= 5)
        {
            const char *cbuf = buf.c_str();
            int size = buf.size();
            im->write_file(id, cbuf, size);
        }
        return extent_protocol::OK;
    }
    return extent_protocol::IOERR;
}

int extent_server_cache::get(extent_protocol::extentid_t id, std::string &buf)
{
    printf("extent_server_cache: get %lld\n", id);

    std::map<extent_protocol::extentid_t, file_entry *>::iterator iter = files.find(id);
    if(iter == files.end())
    {
        id &= 0x7fffffff;

        int size = 0;
        char *cbuf = NULL;

        im->read_file(id, &cbuf, &size);
        if (size == 0)
            buf = "";
        else
        {
            buf.assign(cbuf, size);
            free(cbuf);
        }

        file_entry *file = new file_entry();
        file->buf = buf;
        files.insert(std::pair<extent_protocol::extentid_t, file_entry *>(id, file));
        return extent_protocol::OK;
    }
    else if (iter->second->stale_file == 0)
    {
        buf = iter->second->buf;
        return extent_protocol::OK;
    }
    return extent_protocol::IOERR;
}

int extent_server_cache::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
    printf("extent_server_cache: getattr %lld\n", id);

    std::map<extent_protocol::extentid_t, attr_entry *>::iterator iter = attrs.find(id);
    if (iter != attrs.end())
    {
        a = iter->second->attr;
        return extent_protocol::OK;
    }
    else
    {
        id &= 0x7fffffff;

        extent_protocol::attr attr;
        memset(&attr, 0, sizeof(attr));
        im->getattr(id, attr);
        a = attr;

        attr_entry *entry = new attr_entry();
        entry->attr = attr;
        attrs.insert(std::pair<extent_protocol::extentid_t, attr_entry *>(id, entry));

        return extent_protocol::OK;
    }
    return extent_protocol::IOERR;
}

int extent_server_cache::remove(extent_protocol::extentid_t id, int &)
{
    printf("extent_server_cache: write %lld\n", id);

    std::map<extent_protocol::extentid_t, file_entry *>::iterator iter = files.find(id);
    if(iter == files.end())
    {
        file_entry *file = new file_entry();
        file->stale_file = 1;
        files.insert(std::pair<extent_protocol::extentid_t, file_entry *>(id, file));
    }
    else
    {
        iter->second->stale_file = 1;
    }
    // id &= 0x7fffffff;
    // im->remove_file(id);

    return extent_protocol::OK;
}

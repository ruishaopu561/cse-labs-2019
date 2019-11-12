// RPC stubs for clients to talk to extent_server

#include "extent_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <map>
#include <ctime>

extent_client_cache::extent_client_cache(std::string dst)
{
    sockaddr_in dstsock;
    make_sockaddr(dst.c_str(), &dstsock);
    cl = new rpcc(dstsock);
    if (cl->bind() != 0)
    {
        printf("extent_client_cache: bind failed\n");
    }
}

extent_protocol::status
extent_client_cache::create(uint32_t type, extent_protocol::extentid_t &id)
{
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::create, type, id);
    if (ret == extent_protocol::OK)
    {
        buf_entry *file = new buf_entry();
        files.insert(std::pair<extent_protocol::extentid_t, buf_entry *>(id, file));
    }
    return ret;
}

extent_protocol::status
extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf)
{
    extent_protocol::status ret = extent_protocol::OK;
    std::map<extent_protocol::extentid_t, buf_entry *>::iterator iter = files.find(eid);
    if (iter == files.end())
    {
        ret = cl->call(extent_protocol::get, eid, buf);
        while (ret != extent_protocol::OK)
        {
            ret = cl->call(extent_protocol::get, eid, buf);
        }
        buf_entry *file = new buf_entry();
        file->buf = buf;
        files.insert(std::pair<extent_protocol::extentid_t, buf_entry *>(eid, file));
    }
    else
    {
        buf = iter->second->buf;
    }
    return ret;
}

extent_protocol::status
extent_client_cache::getattr(extent_protocol::extentid_t eid, extent_protocol::attr &attr)
{
    extent_protocol::status ret = extent_protocol::OK;
    std::map<extent_protocol::extentid_t, attr_entry *>::iterator iter = attrs.find(eid);
    if (iter == attrs.end())
    {
        ret = cl->call(extent_protocol::getattr, eid, attr);
        while (ret != extent_protocol::OK)
        {
            ret = cl->call(extent_protocol::getattr, eid, attr);
        }
        attr_entry *new_attr = new attr_entry();
        new_attr->attr = attr;
        attrs.insert(std::pair<extent_protocol::extentid_t, attr_entry *>(eid, new_attr));
    }
    else
    {
        attr = iter->second->attr;
    }
    return ret;
}

extent_protocol::status
extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf)
{
    int r = 0;
    extent_protocol::status ret = extent_protocol::OK;
    std::map<extent_protocol::extentid_t, buf_entry *>::iterator iter = files.find(eid);
    std::map<extent_protocol::extentid_t, attr_entry *>::iterator iter1 = attrs.find(eid);

    if (iter1 != attrs.end())
    {
        iter1->second->attr.size = buf.size();
        iter1->second->attr.atime = (unsigned)std::time(0);
        iter1->second->attr.mtime = (unsigned)std::time(0);
    }

    if (iter == files.end())
    {
        buf_entry *file = new buf_entry();
        file->buf = buf;
        file->buf_put_time = 1;
        files.insert(std::pair<extent_protocol::extentid_t, buf_entry *>(eid, file));
    }
    else
    {
        iter->second->buf = buf;
        iter->second->buf_put_time++;
        if (iter->second->buf_put_time >= 5)
        {
            ret = cl->call(extent_protocol::put, eid, buf, r);
            while (ret != extent_protocol::OK)
            {
                ret = cl->call(extent_protocol::put, eid, buf, r);
            }
            iter->second->buf_put_time = 0;
        }
    }
    return ret;
}

extent_protocol::status
extent_client_cache::remove(extent_protocol::extentid_t eid)
{
    int r = 0;
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::remove, eid, r);
    return ret;
}
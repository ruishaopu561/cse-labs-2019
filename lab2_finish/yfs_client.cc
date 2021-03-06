// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  lc->acquire(1);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  lc->release(1);
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE)
    {
        // printf("isfile: %lld is a file\n", inum);
        return true;
    }
    // printf("isfile: %lld is a dir\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;
    
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR)
    {
        // printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    // printf("isdir: %lld is not a dir\n", inum);
    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;
    
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        lc->release(inum);
        goto release;
    }
    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    lc->release(inum);    
    // printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;  
        lc->release(inum);
        goto release;
    }
    lc->release(inum);
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

#define EXT_RPC(xx)                                                \
    do                                                             \
    {                                                              \
        if ((xx) != extent_protocol::OK)                           \
        {                                                          \
            printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
            r = IOERR;                                             \
            goto release;                                          \
        }                                                          \
    } while (0)

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size)
{
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    if (ino <= 0 || size < 0)
    {
        return IOERR;
    }

    std::string content;
    lc->acquire(ino);
    if (ec->get(ino, content) != extent_protocol::OK)
    {
        lc->release(ino);
        return IOERR;
    }

    content.resize(size);
    
    if (ec->put(ino, content) != extent_protocol::OK)
    {
        lc->release(ino);
        return IOERR;
    }

    lc->release(ino);
    return OK;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found = false;
    lc->acquire(parent);
    lookup(parent, name, found, ino_out);
    if (found)
    {
        lc->release(parent);
        return EXIST;
    }

    if ((ec->create(extent_protocol::T_FILE, ino_out)) != extent_protocol::OK)
    {        
        lc->release(parent);
        return IOERR;
    }
    
    std::list<dirent> dirs;
    if (readdir(parent, dirs) != OK)
    {
        lc->release(parent);
        return IOERR;
    }

    dirent entry;
    entry.name = name;
    entry.inum = ino_out;
    dirs.push_back(entry);
    if (writedir(parent, dirs) != OK)
    {        
        lc->release(parent);
        return IOERR;
    }

    lc->release(parent);        
    return OK;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found = false;
    lc->acquire(parent);
    lookup(parent, name, found, ino_out);
    if (found)
    {
        lc->release(parent);
        return EXIST;
    }

    if ((ec->create(extent_protocol::T_DIR, ino_out)) != extent_protocol::OK)
    {
        lc->release(parent);
        return IOERR;
    }

    std::list<dirent> dirs;
    if (readdir(parent, dirs) != OK)
    {
        lc->release(parent);
        return IOERR;
    }

    dirent entry;
    entry.name = name;
    entry.inum = ino_out;
    dirs.push_back(entry);
    if (writedir(parent, dirs) != OK)
    {
        lc->release(parent);
        return IOERR;
    }

    lc->release(parent);
    return OK;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    found = false;
    if (!isdir(parent))
    {
        return IOERR;
    }

    std::list<dirent> dirs;
    if (readdir(parent, dirs) != OK)
    {
        return IOERR;
    }

    found = false;
    for (std::list<dirent>::iterator iter = dirs.begin(); iter != dirs.end(); iter++)
    {
        if (iter->name == name)
        {
            ino_out = iter->inum;
            found = true;
            return OK;
        }
    }
    return OK;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string content;
    r = ec->get(dir, content);
    if (r != extent_protocol::OK)
    {
        return IOERR;
    }

    list.clear();
    std::istringstream iss(content);
    dirent entry;

    while (std::getline(iss, entry.name, '\0'))
    {
        iss >> entry.inum;
        list.push_back(entry);
    }
    // printf("readdir %d entries: %d.\n", dir, list.size());
    return OK;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    /*
     * your code goes here.
     * note: read using ec->get().
     */
    if (ino <= 0 || size < 0 || off < 0)
    {
        return IOERR;
    }

    std::string content;
    lc->acquire(ino);
    if (ec->get(ino, content) != extent_protocol::OK)
    {
        lc->release(ino);
        return IOERR;
    }

    if ((unsigned)off >= content.size())
    {
        data.erase();
        lc->release(ino);
        return IOERR;
    }

    int length = (unsigned)size > content.size() - off ? content.size() - off : size;
    data = content.substr(off, length);

    lc->release(ino);
    return OK;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written)
{
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    if (ino <= 0 || size < 0 || off < 0)
    {
        return IOERR;
    }

    std::string content;
    lc->acquire(ino);
    if (ec->get(ino, content) != extent_protocol::OK)
    {
        lc->release(ino);
        return IOERR;
    }

    int out = (int)off + (int)size - (int)content.size();
    if (out >= 0)
    {
        for (int i = 0; i < out; i++)
        {
            content += '\0';
        }
    }

    for (int i = 0; i < (int)size; i++)
    {
        content[off + i] = *(char *)(data + i);
    }

    if (ec->put(ino, content) != extent_protocol::OK)
    {
        lc->release(ino);
        return IOERR;
    }

    lc->release(ino);
    return OK;
}

int yfs_client::unlink(inum parent, const char *name)
{
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    std::list<dirent> dirs;

    lc->acquire(parent);
    if (readdir(parent, dirs) != OK)
    {
        lc->release(parent);
        return IOERR;
    }

    std::list<dirent>::iterator iter;
    for (iter = dirs.begin(); iter != dirs.end(); iter++)
    {
        if (iter->name == name)
        {
            break;
        }
    }

    if (iter == dirs.end())
    {
        lc->release(parent);
        return IOERR;
    }

    if (!isfile(iter->inum))
    {
        lc->release(parent);
        return IOERR;
    }

    if (ec->remove(iter->inum) != extent_protocol::OK)
    {
        lc->release(parent);
        return IOERR;
    }
    
    dirs.erase(iter);

    if (writedir(parent, dirs) != OK)
    {
        lc->release(parent);
        return IOERR;
    }

    lc->release(parent);
    return OK;
}

int yfs_client::writedir(inum ino, std::list<dirent> &list)
{

    std::ostringstream oss;

    for (std::list<dirent>::iterator iter = list.begin(); iter != list.end(); iter++)
    {
        oss << iter->name;
        oss.put('\0');
        oss << iter->inum;
    }
    // printf("writedir %d entries: %d.\n", ino, list.size());

    if (ec->put(ino, oss.str()) != extent_protocol::OK)
    {
        return IOERR;
    }

    return OK;
}

int yfs_client::readlink(inum ino, std::string &link)
{
    lc->acquire(ino);
    if (ec->get(ino, link) != extent_protocol::OK)
    {
        lc->release(ino);
        return IOERR;
    }

    lc->release(ino);
    return OK;
}

int yfs_client::symlink(inum parent, const char *actualpath, const char *sympath, inum &ino_out)
{
    bool found = false;

    lc->acquire(parent);
    lookup(parent, actualpath, found, ino_out);
    if (found)
    {
        lc->release(parent);
        return EXIST;
    }

    if (ec->create(extent_protocol::T_SYMLINK, ino_out) != extent_protocol::OK)
    {
        lc->release(parent);
        return IOERR;
    }

    if (ec->put(ino_out, std::string(sympath)) != extent_protocol::OK)
    {
        lc->release(parent);
        return IOERR;
    }

    std::list<dirent> dirs;
    if (readdir(parent, dirs) != OK)
    {
        lc->release(parent);
        return IOERR;
    }

    dirent dir;
    dir.name = actualpath;
    dir.inum = ino_out;
    dirs.push_back(dir);

    if (writedir(parent, dirs) != OK)
    {
        lc->release(parent);
        return IOERR;
    }

    lc->release(parent);
    return OK;
}

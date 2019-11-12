#include "inode_manager.h"
#include <ctime>

// disk layer -----------------------------------------

disk::disk()
{
    bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
    if (id >= BLOCK_NUM || id <= 0 || !buf)
    {
        return;
    }
    memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
    if (id >= BLOCK_NUM || id <= 0 || !buf)
    {
        return;
    }
    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
    /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
    uint32_t this_block = IBLOCK(INODE_NUM, sb.nblocks) + 1;
    for (; this_block < BLOCK_NUM; this_block++)
    {
        std::map<uint32_t, int>::iterator iter = using_blocks.find(this_block);
        if (iter == using_blocks.end())
        {
            using_blocks.insert(std::pair<uint32_t, int>(this_block, 1));
            return this_block;
        }
        else if (iter->second == 0)
        {
            iter->second = 1;
            return this_block;
        }
    }
    printf("block_manager::alloc_block error: Blocks are run out of.\n");
    exit(0);
}

void block_manager::free_block(uint32_t id)
{
    /* 
     * your code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */
    std::map<uint32_t, int>::iterator iter = using_blocks.find(id);
    if (iter == using_blocks.end())
    {
        return;
    }
    iter->second = 0;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

    /* 
   * Something hard to debug will happen 
   * if l didn't initial the disk
   */

    /* Alloc boot block */
    alloc_block();

    /* Alloc super block */
    alloc_block();

    /* Alloc bitmap */
    uint32_t i;
    for (i = 0; i < BLOCK_NUM / BPB; i++)
    {
        alloc_block();
    }

    /* Alloc inode table */
    for (i = 0; i < INODE_NUM / IPB; i++)
    {
        alloc_block();
    }
}

void block_manager::read_block(uint32_t id, char *buf)
{
    d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{
    d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
    bm = new block_manager();
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1)
    {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
    /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */

    inode_t *node = NULL;
    uint32_t this_num = 1;
    for (; this_num < INODE_NUM; this_num++)
    {
        node = get_inode(this_num);
        if (node == NULL)
        {
            node = (inode_t *)malloc(sizeof(inode_t));
            break;
        }
        else if (node->type == 0)
        {
            break;
        }
        free(node);
    }
    if (!node)
    {
        printf("im::alloc_inode ERROR: cannot allocate inode, will exit\n");
        return 0;
    }
    node->type = type;
    node->size = 0;
    node->atime = (unsigned)std::time(0);
    node->mtime = (unsigned)std::time(0);
    node->ctime = (unsigned)std::time(0);

    put_inode(this_num, node);
    free(node);
    return this_num;
}

void inode_manager::free_inode(uint32_t inum)
{
    /* 
     * your code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     */
    inode_t *node = get_inode(inum);
    if (node == NULL)
    {
        printf("im::free_inode ERROR: inode returned by get_inode() is NULL, will exit\n");
        return;
    }

    if (node->type == 0)
    {
        printf("im::free_inode WARNING: the inode is already a freed one\n");
        return;
    }
    node->type = 0;
    put_inode(inum, node);
    free(node);
    return;
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum)
{
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    // printf("\tim: get_inode %d\n", inum);

    if (inum < 0 || inum >= INODE_NUM)
    {
        printf("\tim: inum out of range\n");
        return NULL;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode *)buf + inum % IPB;
    if (ino_disk->type == 0)
    {
        // printf("\tim: inode not exist\n");
        return NULL;
    }

    ino = (struct inode *)malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    // printf("\tim: put_inode %d\n", inum);
    if (ino == NULL)
        return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *)buf + inum % IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* 
 * Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller.
 */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
    /*
     * your code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_Out
     */
    inode_t *node = get_inode(inum);
    if (!node)
    {
        return;
    }

    *size = node->size;
    if (node->size == 0)
    {
        *buf_out = NULL;
        return;
    }

    int block_sum = (node->size - 1) / BLOCK_SIZE + 1;

    char *dest = (char *)malloc(block_sum * BLOCK_SIZE);

    for (int i = 0; i < MIN(block_sum, NDIRECT); i++)
    {
        blockid_t id = node->blocks[i];
        bm->read_block(id, dest + i * BLOCK_SIZE);
    }
    if (block_sum > NDIRECT)
    {
        blockid_t blockblock[NINDIRECT];
        bm->read_block(node->blocks[NDIRECT], (char *)blockblock);
        for (int i = NDIRECT; i < block_sum; i++)
        {
            blockid_t id = blockblock[i - NDIRECT];
            bm->read_block(id, dest + i * BLOCK_SIZE);
        }
    }
    *buf_out = dest;
    node->atime = (unsigned)std::time(0);
    node->ctime = (unsigned)std::time(0);
    put_inode(inum, node);
    free(node);
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
    /*
     * your code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf 
     * is larger or smaller than the size of original inode
     */
    if (size < 0 || (unsigned)size > MAXFILE * BLOCK_SIZE)
    {
        return;
    }

    inode_t *node = get_inode(inum);

    int old_sum = node->size == 0 ? 0 : (node->size - 1) / BLOCK_SIZE + 1;
    int new_sum = size == 0 ? 0 : (size - 1) / BLOCK_SIZE + 1;

    if (old_sum < new_sum)
    {
        if (old_sum >= NDIRECT)
        {
            blockid_t blockblock[NINDIRECT];
            bm->read_block(node->blocks[NDIRECT], (char *)blockblock);
            for (int i = old_sum; i < new_sum; i++)
            {
                blockid_t id = bm->alloc_block();
                blockblock[i - NDIRECT] = id;
            }
            bm->write_block(node->blocks[NDIRECT], (char *)blockblock);
        }
        else if (new_sum <= NDIRECT)
        {
            for (int i = old_sum; i < new_sum; i++)
            {
                node->blocks[i] = bm->alloc_block();
            }
        }
        else
        {
            for (int i = old_sum; i < NDIRECT; i++)
            {
                node->blocks[i] = bm->alloc_block();
            }
            node->blocks[NDIRECT] = bm->alloc_block();

            blockid_t blockblock[NINDIRECT];
            bm->read_block(node->blocks[NDIRECT], (char *)blockblock);
            for (int i = NDIRECT; i < new_sum; i++)
            {
                blockid_t id = bm->alloc_block();
                blockblock[i - NDIRECT] = id;
            }
            bm->write_block(node->blocks[NDIRECT], (char *)blockblock);
        }
    }
    else if (old_sum > new_sum)
    {
        if (old_sum <= NDIRECT)
        {
            for (int i = new_sum; i < old_sum; i++)
            {
                bm->free_block(node->blocks[i]);
                node->blocks[i] = 0;
            }
        }
        else if (new_sum >= NDIRECT)
        {
            blockid_t blockblock[NINDIRECT];
            bm->read_block(node->blocks[NDIRECT], (char *)blockblock);
            for (int i = new_sum; i < old_sum; i++)
            {
                blockid_t id = blockblock[i - NDIRECT];
                bm->free_block(id);
                blockblock[i - NDIRECT] = 0;
            }
            bm->write_block(node->blocks[NDIRECT], (char *)blockblock);
        }
        else
        {
            blockid_t blockblock[NINDIRECT];
            bm->read_block(node->blocks[NDIRECT], (char *)blockblock);
            for (int i = NDIRECT; i < old_sum; i++)
            {
                blockid_t id = blockblock[i - NDIRECT];
                bm->free_block(id);
                blockblock[i - NDIRECT] = 0;
            }
            bm->write_block(node->blocks[NDIRECT], (char *)blockblock);

            for (int i = new_sum; i <= NDIRECT; i++)
            {
                bm->free_block(node->blocks[i]);
                node->blocks[i] = 0;
            }
        }
    }

    for (int i = 0; i < MIN(new_sum, NDIRECT); i++)
    {
        uint32_t id = node->blocks[i];
        bm->write_block(id, buf + i * BLOCK_SIZE);
    }
    if (new_sum > NDIRECT)
    {
        blockid_t blockblock[NINDIRECT];
        bm->read_block(node->blocks[NDIRECT], (char *)blockblock);
        for (int i = NDIRECT; i < new_sum; i++)
        {
            blockid_t id = blockblock[i - NDIRECT];
            bm->write_block(id, buf + i * BLOCK_SIZE);
        }
    }
    node->size = (unsigned int)size;
    node->atime = (unsigned)std::time(0);
    node->mtime = (unsigned)std::time(0);
    node->ctime = (unsigned)std::time(0);
    put_inode(inum, node);
    free(node);
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
    /*
     * your code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */
    inode_t *node = get_inode(inum);
    if (!node)
    {
        a.type = 0;
        free(node);
        return;
    }
    a.atime = node->atime;
    a.ctime = node->ctime;
    a.mtime = node->mtime;
    a.type = (uint32_t)node->type;
    a.size = node->size;
    free(node);
}

void inode_manager::remove_file(uint32_t inum)
{
    /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
    inode_t *node = get_inode(inum);
    if (!node)
    {
        return;
    }
    int block_sum = node->size == 0 ? 0 : ((node->size - 1) / BLOCK_SIZE + 1);
    if (block_sum <= NDIRECT)
    {
        for (int i = 0; i < block_sum; i++)
        {
            blockid_t id = node->blocks[i];
            bm->free_block(id);
        }
    }
    else
    {
        for (int i = 0; i < NDIRECT; i++)
        {
            blockid_t id = node->blocks[i];
            bm->free_block(id);
        }
        blockid_t blockblock[NINDIRECT];
        bm->read_block(node->blocks[NDIRECT], (char *)blockblock);
        for (int i = NDIRECT; i < block_sum; i++)
        {
            blockid_t id = blockblock[i - NDIRECT];
            bm->free_block(id);
        }
        bm->free_block(node->blocks[NDIRECT]);
    }
    free_inode(inum);
    free(node);
}

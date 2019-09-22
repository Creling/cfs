#include "CFS.h"

extern struct file_operations CFS_file_ops;
extern struct file_operations CFS_dir_ops;
extern struct inode_operations CFS_inode_ops;
extern struct address_space_operations CFS_aops;

/* 将CFS inode赋值到 VFS inode */
void CFS_convert_inode(struct CFS_inode *H_inode, struct inode *vfs_inode)
{
    vfs_inode->i_ino = H_inode->inode_no;
    vfs_inode->i_mode = H_inode->mode;
    vfs_inode->i_size = H_inode->file_size;
    set_nlink(vfs_inode, H_inode->i_nlink);
    i_uid_write(vfs_inode, H_inode->i_uid);
    i_gid_write(vfs_inode, H_inode->i_gid);
    vfs_inode->i_atime.tv_sec = H_inode->i_atime;
    vfs_inode->i_ctime.tv_sec = H_inode->i_ctime;
    vfs_inode->i_mtime.tv_sec = H_inode->i_mtime;
}

/* 获取sb指向的super block中序号为inode_no的inode并储存在raw_inode指向的内存中 */
int CFS_get_inode(struct super_block *sb, uint64_t inode_no, struct CFS_inode *raw_inode)
{
    if (!raw_inode)
    {
        printk(KERN_ERR "inode is null");
        return -1;
    }
    struct CFS_super_block *H_sb = sb->s_fs_info;
    struct CFS_inode *H_inode_array = NULL;

    int i;
    struct buffer_head *bh;
    bh = sb_bread(sb,
                  H_sb->inode_table_block_index + inode_no * sizeof(struct CFS_inode) / CFS_BLOCKSIZE);
    printk(KERN_INFO "H_sb->inode_table_block_index is %lld",
           H_sb->inode_table_block_index);
    BUG_ON(!bh);

    H_inode_array = (struct CFS_inode *)bh->b_data;
    int idx = inode_no % (CFS_BLOCKSIZE / sizeof(struct CFS_inode));
    ssize_t inode_array_size = CFS_BLOCKSIZE / sizeof(struct CFS_inode);
    if (idx > inode_array_size)
    {
        printk(KERN_ERR "in get_inode: out of index");
        return -1;
    }
    memcpy(raw_inode, H_inode_array + idx, sizeof(struct CFS_inode));
    if (raw_inode->inode_no != inode_no)
    {
        printk(KERN_ERR "inode not init");
    }
    return 0;
}

/* 以H_inode.inode_no为序号将H_inode存入sb指向的super block */
int save_inode(struct super_block *sb, struct CFS_inode H_inode)
{
    uint64_t inode_num = H_inode.inode_no;
    struct CFS_super_block *disk_sb = sb->s_fs_info;
    uint64_t block_idx = inode_num * sizeof(struct CFS_inode) / CFS_BLOCKSIZE + disk_sb->inode_table_block_index;
    uint64_t arr_off = inode_num % (CFS_BLOCKSIZE / sizeof(struct CFS_inode));

    //1. read disk inode
    struct buffer_head *bh;
    bh = sb_bread(sb, block_idx);
    printk(KERN_ERR "In save inode and inode_no is %llu and block_idx is %llu and bh is %llu\n",
           inode_num, block_idx, sb);
    BUG_ON(!bh);

    //2. change disk inode
    struct CFS_inode *p_disk_inode;
    p_disk_inode = (struct CFS_inode *)bh->b_data;
    memcpy(p_disk_inode + arr_off, &H_inode, sizeof(H_inode));

    //3. save disk inode
    map_bh(bh, sb, block_idx);
    brelse(bh);
    return 0;
}

ssize_t CFS_write_inode_data(struct inode *inode, const void *buf, size_t count)
{
    struct super_block *sb;
    struct CFS_inode H_inode;

    sb = inode->i_sb;

    if (!buf)
    {
        printk(KERN_ERR
               "HUST: buf is null\n");
        return -EFAULT;
    }
    if (count > CFS_BLOCKSIZE * CFS_FILE_MAX_BLOCKS)
    {
        return -ENOSPC;
    }

    if (-1 == CFS_get_inode(sb, inode->i_ino, &H_inode))
    {
        printk(KERN_ERR
               "HUST: cannot read inode\n");
        return -EFAULT;
    }
    if (count > CFS_BLOCKSIZE * H_inode.blocks)
    {
        int ret;
        ret = alloc_block_for_inode(sb, &H_inode,
                                    (count - CFS_BLOCKSIZE * H_inode.blocks) / CFS_BLOCKSIZE);
        if (ret)
        {
            return -EFAULT;
        }
        mark_inode_dirty(inode);
    }
    size_t count_res = count;
    int i;
    i = 0;
    while (count_res && i < CFS_FILE_MAX_BLOCKS)
    {
        struct buffer_head *bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        size_t cpy_size;
        if (count_res >= CFS_BLOCKSIZE)
        {
            count_res -= CFS_BLOCKSIZE;
            cpy_size = CFS_BLOCKSIZE;
        }
        else
        {
            count_res = 0;
            cpy_size = count_res;
        }
        memcpy(bh->b_data, buf + i * CFS_BLOCKSIZE, cpy_size);
        map_bh(bh, sb, H_inode.block[i]);
        i++;
        brelse(bh);
    }
    while (i < H_inode.blocks)
    {
        struct buffer_head *bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        memset(bh->b_data, 0, CFS_BLOCKSIZE);
        map_bh(bh, sb, H_inode.block[i]);
        brelse(bh);
        i++;
    }
    return count;
}

ssize_t CFS_read_inode_data(struct inode *inode, void *buf, size_t size)
{
    if (!buf)
    {
        printk(KERN_ERR "HUST: buf is null\n");
        return 0;
    }
    memset(buf, 0, size);
    struct super_block *sb = inode->i_sb;
    printk(KERN_INFO "HUST: read inode [%lu]\n", inode->i_ino);
    struct CFS_inode H_inode;
    if (-1 == CFS_get_inode(sb, inode->i_ino, &H_inode))
    {
        printk(KERN_ERR "HUST: cannot read inode\n");
        return -EFAULT;
    }
    int i;
    for (i = 0; i < H_inode.blocks; ++i)
    {
        struct buffer_head *bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        if ((i + 1) * CFS_BLOCKSIZE > size)
        {
            brelse(bh);
            return i * CFS_BLOCKSIZE;
        }
        memcpy(buf + i * (CFS_BLOCKSIZE), bh->b_data, CFS_BLOCKSIZE);
        brelse(bh);
    }
    return i * (CFS_BLOCKSIZE);
}

int CFS_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    struct super_block *sb = dir->i_sb;
    struct CFS_super_block *disk_sb = sb->s_fs_info;
    printk(KERN_ERR "In create obj and dir is %llu\n", (uint64_t)dir);
    const unsigned char *name = dentry->d_name.name;

    struct CFS_inode H_dir_inode;
    CFS_get_inode(sb, dir->i_ino, &H_dir_inode);

    if (H_dir_inode.dir_children_count >= CFS_BLOCKSIZE / sizeof(struct CFS_dir_record))
    {
        return -ENOSPC;
    }
    //1. write inode
    uint64_t first_empty_inode_num = CFS_get_empty_inode(dir->i_sb);
    BUG_ON(!first_empty_inode_num);
    struct inode *inode;
    struct CFS_inode raw_inode;
    inode = new_inode(sb);
    if (!inode)
    {
        return -ENOSPC;
    }
    inode->i_ino = first_empty_inode_num;
    raw_inode.inode_no = first_empty_inode_num;
    inode_init_owner(inode, dir, mode);
    inode->i_op = &CFS_inode_ops;
    raw_inode.i_uid = i_uid_read(inode);
    raw_inode.i_gid = i_gid_read(inode);
    raw_inode.i_nlink = inode->i_nlink;
    struct timespec current_time;
    getnstimeofday(&current_time);
    inode->i_mtime = inode->i_atime = inode->i_ctime = current_time;
    raw_inode.i_atime = (inode->i_atime.tv_sec);
    raw_inode.i_ctime = (inode->i_ctime.tv_sec);
    raw_inode.i_mtime = (inode->i_mtime.tv_sec);

    raw_inode.mode = mode;
    if (S_ISDIR(mode))
    {
        inode->i_size = 1;
        inode->i_blocks = 1;
        inode->i_fop = &CFS_dir_ops;

        raw_inode.blocks = 1;
        raw_inode.dir_children_count = 2;

        //2. write block
        if (disk_sb->free_blocks <= 0)
        {
            return -ENOSPC;
        }
        struct CFS_dir_record dir_arr[2];
        uint64_t first_empty_block_num = CFS_get_empty_block(sb);
        raw_inode.block[0] = first_empty_block_num;
        const char *cur_dir = ".";
        const char *parent_dir = "..";
        memcpy(dir_arr[0].filename, cur_dir, strlen(cur_dir) + 1);
        dir_arr[0].inode_no = first_empty_inode_num;
        memcpy(dir_arr[1].filename, parent_dir, strlen(parent_dir) + 1);
        //dir_arr[2].inode_no = dir->i_ino;
        save_inode(sb, raw_inode);
        save_block(sb, first_empty_block_num, dir_arr, sizeof(struct CFS_dir_record) * 2);
        set_and_save_bmap(sb, first_empty_block_num, 1);

        disk_sb->free_blocks -= 1;
    }
    else if (S_ISREG(mode))
    {
        inode->i_size = 0;
        inode->i_blocks = 0;
        inode->i_fop = &CFS_file_ops;
        inode->i_mapping->a_ops = &CFS_aops;
        raw_inode.blocks = 0;
        raw_inode.file_size = 0;

        //write inode
        save_inode(sb, raw_inode);
    }
    struct CFS_dir_record new_dir;
    memcpy(new_dir.filename, name, strlen(name) + 1);
    new_dir.inode_no = first_empty_inode_num;
    struct buffer_head *bh;
    bh = sb_bread(sb, H_dir_inode.block[0]);
    memcpy(bh->b_data + H_dir_inode.dir_children_count * sizeof(struct CFS_dir_record), &new_dir, sizeof(new_dir));
    map_bh(bh, sb, H_dir_inode.block[0]);
    brelse(bh);

    //updata dir inode
    H_dir_inode.dir_children_count += 1;
    save_inode(sb, H_dir_inode);

    set_and_save_imap(sb, first_empty_inode_num, 1);
    insert_inode_hash(inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);
    d_instantiate(dentry, inode);
    printk(KERN_ERR "first_empty_inode_num is %llu\n", first_empty_inode_num);
    return 0;
}



int save_block(struct super_block* sb, uint64_t block_num, void* buf, ssize_t size)
{
    /*
     * 1. read disk block
     * 2. change block
     * 2.1 TODO verify block
     * 3. save block
     */
    struct CFS_super_block *disk_sb;
    disk_sb = sb->s_fs_info;
    struct buffer_head* bh;
    bh = sb_bread(sb, block_num+disk_sb->data_block_index);
    
    BUG_ON(!bh);
    memset(bh->b_data, 0, CFS_BLOCKSIZE);
    memcpy(bh->b_data, buf, size);
    brelse(bh);
    return 0;
}
int CFS_get_block(struct inode *inode, sector_t block,
		      struct buffer_head *bh, int create)
{
	struct super_block *sb = inode->i_sb;
	
	printk(KERN_INFO "HUST: get block [%lu] of inode [%lu]\n", block,
	       inode->i_ino);
	if (block > CFS_FILE_MAX_BLOCKS) {
		return -ENOSPC;
	}
	struct CFS_inode H_inode;
	if (-1 == CFS_get_inode(sb, inode->i_ino, &H_inode))
		return -EFAULT;
	if (H_inode.blocks == 0){
        if(alloc_block_for_inode(sb, &H_inode, 1)) {
            return -EFAULT;
        }
    }
    mark_inode_dirty(inode);
	map_bh(bh, sb, H_inode.block[0]);
	return 0;
}

int alloc_block_for_inode(struct super_block* sb, struct CFS_inode* p_H_inode, ssize_t size)
{
    struct CFS_super_block* disk_sb;
    ssize_t bmap_size;
    uint8_t* bmap;
    unsigned int i;
    
    ssize_t alloc_blocks = size - p_H_inode->blocks;
    if(size + p_H_inode->blocks > CFS_FILE_MAX_BLOCKS){
        return -ENOSPC;
    }
    //read bmap
    disk_sb = sb->s_fs_info;
    bmap_size = disk_sb->blocks_count/8;
    bmap = kmalloc(bmap_size, GFP_KERNEL);
    
    if(get_bmap(sb, bmap, bmap_size))
    {
        kfree(bmap);
        return -EFAULT;
    }
    
    for(i = 0; i < alloc_blocks; ++i) {
        uint64_t empty_blk_num = CFS_find_first_zero_bit(bmap, disk_sb->blocks_count / 8);
        p_H_inode->block[p_H_inode->blocks] = empty_blk_num;
        p_H_inode->blocks++;
        uint64_t bit_off = empty_blk_num % (CFS_BLOCKSIZE*8);
        setbit(bmap[bit_off/8], bit_off%8);
    }
    save_bmap(sb,bmap,bmap_size);
    save_inode(sb,*p_H_inode);
    disk_sb->free_blocks -= size;
    kfree(bmap);
    return 0;
}

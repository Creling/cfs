
#include "CFS.h"
#include <linux/time.h>

extern struct file_operations CFS_file_ops;
extern struct file_operations CFS_dir_ops;
extern struct inode_operations CFS_inode_ops;
extern struct address_space_operations CFS_aops;

int CFS_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    return CFS_create_obj(dir, dentry, mode);
}

int CFS_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    return CFS_create_obj(dir, dentry, S_IFDIR | mode);
}

int CFS_unlink(struct inode *dir, struct dentry *dentry)
{
    struct super_block *sb = dir->i_sb;
    printk(KERN_INFO "HUST: unlink [%s] from dir inode [%lu]\n",
           dentry->d_name.name, dir->i_ino);
    struct CFS_inode H_dir_inode;
    if (CFS_get_inode(sb, dir->i_ino, &H_dir_inode))
    {
        return -EFAULT;
    }
    ssize_t buf_size = H_dir_inode.blocks * CFS_BLOCKSIZE;
    void *buf = kmalloc(buf_size, GFP_KERNEL);
    if (CFS_read_inode_data(dir, buf, buf_size) != buf_size)
    {
        printk(KERN_ERR "CFS_read_inode_data failed\n");
        kfree(buf);
        return -EFAULT;
    }
    struct inode *inode = d_inode(dentry);
    int i;
    struct CFS_dir_record *p_dir;
    p_dir = (struct CFS_dir_record *)buf;
    for (i = 0; i < H_dir_inode.dir_children_count; ++i)
    {
        if (strncmp(dentry->d_name.name, p_dir[i].filename, CFS_FILENAME_MAX_LEN))
        {
            /* We have found our directory entry. We can now clear i
             * and then decrease the inode's link count. 
             */
            H_dir_inode.dir_children_count -= 1;
            //remove it from buf
            struct CFS_dir_record *new_buf = kmalloc(buf_size - sizeof(struct CFS_dir_record), GFP_KERNEL);
            memcpy(new_buf, p_dir, (i) * sizeof(struct CFS_dir_record));
            memcpy(new_buf + i, p_dir + i + 1,
                   (H_dir_inode.dir_children_count - i - 1) * sizeof(struct CFS_dir_record));
            CFS_write_inode_data(dir, new_buf, buf_size - sizeof(struct CFS_dir_record));
            kfree(new_buf);
            break;
        }
    }
    inode_dec_link_count(inode);
    mark_inode_dirty(inode);
    kfree(buf);
    save_inode(sb, H_dir_inode);
    return 0;
}

struct dentry *CFS_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags)
{
    struct super_block *sb = parent_inode->i_sb;
    struct CFS_inode H_inode;
    struct inode *inode = NULL;
    uint64_t data_block = 0, i;
    struct CFS_dir_record *dtptr;
    struct buffer_head *bh;

    printk(KERN_ERR "CFS: lookup [%s] in inode [%lu]\n",
           child_dentry->d_name.name, parent_inode->i_ino);

    if (-1 == CFS_get_inode(sb, parent_inode->i_ino, &H_inode))
    {
        printk(KERN_ERR "HUST: cannot get inode\n");
        return ERR_PTR(-EFAULT);
    }

    data_block = H_inode.block[0];
    bh = sb_bread(sb, data_block);
    if (!bh)
    {
        printk(KERN_ERR
               "1bfs lookup: Could not read data block [%llu]\n",
               data_block);
        return ERR_PTR(-EFAULT);
    }

    dtptr = (struct CFS_dir_record *)bh->b_data;

    for (i = 0; i < H_inode.dir_children_count; i++)
    {
        printk(KERN_ERR "child_dentry is %s and file name is %s\n", child_dentry->d_name.name, dtptr[i].filename);
        if (strncmp(child_dentry->d_name.name, dtptr[i].filename,
                    CFS_FILENAME_MAX_LEN) == 0)
        {

            printk(KERN_ERR "in case 1");
            inode = iget_locked(sb, dtptr[i].inode_no);
            if (!inode)
            {
                printk(KERN_ERR
                       "CFS lookup: iget_locked() returned NULL\n");
                brelse(bh);
                return ERR_PTR(-EFAULT);
            }

            if (inode->i_state & I_NEW)
            {
                inode_init_owner(inode, parent_inode, 0);

                struct CFS_inode H_child_inode;
                if (-1 == CFS_get_inode(sb, dtptr[i].inode_no, &H_child_inode))
                {
                    return ERR_PTR(-EFAULT);
                }

                CFS_convert_inode(&H_child_inode, inode);
                printk(KERN_ERR "uid is %lu and gid is %lu", inode->i_uid, inode->i_gid);
                inode->i_op = &CFS_inode_ops;

                if (S_ISDIR(H_child_inode.mode))
                {
                    inode->i_fop = &CFS_dir_ops;
                }
                else if (S_ISREG(H_child_inode.mode))
                {
                    inode->i_fop = &CFS_file_ops;
                    ;
                    inode->i_mapping->a_ops = &CFS_aops;
                }
                inode->i_mode = H_child_inode.mode;
                inode->i_size = H_child_inode.file_size;
                insert_inode_hash(inode);
                printk(KERN_ERR "inode->i_sb is %llu and sb is %llu\n", (uint64_t)inode->i_sb, (uint64_t)sb);
                unlock_new_inode(inode);
            }

            d_add(child_dentry, inode);
            brelse(bh);

            printk(KERN_ERR "lookup over at a\n");
            return NULL;
        }
    }

    d_add(child_dentry, NULL);
    brelse(bh);
    printk(KERN_ERR "lookup over\n");
    return NULL;
}

#include "CFS.h"


extern struct file_system_type CFS_type;
extern struct file_operations CFS_file_ops;
extern struct file_operations CFS_dir_ops;
extern struct inode_operations CFS_inode_ops;
extern struct super_operations CFS_super_ops;
extern struct address_space_operations CFS_aops;

int save_super(struct super_block *sb)
{
	struct CFS_super_block *disk_sb = sb->s_fs_info;
	struct buffer_head *bh;
	bh = sb_bread(sb, 1);
	printk(KERN_ERR "In save bmap\n");
	memcpy(bh->b_data, disk_sb, CFS_BLOCKSIZE);
	map_bh(bh, sb, 1);
	brelse(bh);
	return 0;
}

int CFS_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret = -EPERM;
	struct buffer_head *bh;
	bh = sb_bread(sb, 1);
	BUG_ON(!bh);
	struct CFS_super_block *sb_disk;
	sb_disk = (struct CFS_super_block *)bh->b_data;

	printk(KERN_INFO "CFS: version num is %llu\n", sb_disk->version);
	printk(KERN_INFO "CFS: magic_number num is %llu\n", sb_disk->magic_number);
	printk(KERN_INFO
		   "CFS: block_size num is %llu\n",
		   sb_disk->block_size);
	printk(KERN_INFO
		   "CFS: inodes_count num is %llu\n",
		   sb_disk->inodes_count);
	printk(KERN_INFO
		   "CFS: free_blocks num is %llu\n",
		   sb_disk->free_blocks);
	printk(KERN_INFO
		   "CFS: blocks_count num is %llu\n",
		   sb_disk->blocks_count);

	if (sb_disk->magic_number != magic_number_NUM)
	{
		printk(KERN_ERR
			   "magic_number number not match!\n");
		goto release;
	}

	struct inode *root_inode;

	if (sb_disk->block_size != 4096)
	{
		printk(KERN_ERR
			   "CFS expects a blocksize of %d\n",
			   4096);
		ret = -EFAULT;
		goto release;
	}
	//fill vfs super block
	sb->s_magic = sb_disk->magic_number;
	sb->s_fs_info = sb_disk;
	sb->s_maxbytes = CFS_BLOCKSIZE * CFS_FILE_MAX_BLOCKS; /* Max file size */
	sb->s_op = &CFS_super_ops;

	//-----------test get inode-----
	struct CFS_inode raw_root_node;
	if (CFS_get_inode(sb, CFS_ROOT_INODE_NUM, &raw_root_node) != -1)
	{
		printk(KERN_INFO
			   "Get inode sucessfully!\n");
		printk(KERN_INFO
			   "root blocks is %llu and block[0] is %llu\n",
			   raw_root_node.blocks, raw_root_node.block[0]);
	}
	//-----------end test-----------

	root_inode = new_inode(sb);
	if (!root_inode)
		return -ENOMEM;

	/* Our root inode. It doesn't contain useful information for now.
	 * Note that i_ino must not be 0, since valid inode numbers start at
	 * 1. */
	inode_init_owner(root_inode, NULL, S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	root_inode->i_sb = sb;
	root_inode->i_ino = CFS_ROOT_INODE_NUM;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime =
		current_time(root_inode);

	root_inode->i_mode = raw_root_node.mode;
	root_inode->i_size = raw_root_node.dir_children_count;
	//root_inode->i_private = CFS_get_inode(sb, CFS_ROOT_INODE_NUM);
	/* Doesn't really matter. Since this is a directory, it "should"
	 * have a link count of 2. See btrfs, though, where directories
	 * always have a link count of 1. That appears to work, even though
	 * it created a number of bug reports in other tools. :-) Just
	 * search the web for that topic. */
	inc_nlink(root_inode);

	/* What can you do with our inode? */
	root_inode->i_op = &CFS_inode_ops;
	root_inode->i_fop = &CFS_dir_ops;

	/* Make a struct dentry from our inode and store it in our
	 * superblock. */
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root)
		return -ENOMEM;
release:
	brelse(bh);

	return 0;
}

void CFS_evict_inode(struct inode *vfs_inode)
{
	struct super_block *sb = vfs_inode->i_sb;
	printk(KERN_INFO
		   "HUST evict: Clearing inode [%lu]\n",
		   vfs_inode->i_ino);
	truncate_inode_pages_final(&vfs_inode->i_data);
	clear_inode(vfs_inode);
	if (vfs_inode->i_nlink)
	{
		printk(KERN_INFO
			   "HUST evict: Inode [%lu] still has links\n",
			   vfs_inode->i_ino);
		return;
	}
	printk(KERN_INFO
		   "HUST evict: Inode [%lu] has no links!\n",
		   vfs_inode->i_ino);
	set_and_save_imap(sb, vfs_inode->i_ino, 0);
	return;
}

int CFS_write_inode(struct inode *inode, struct writeback_control *wbc)
{
    struct buffer_head *bh;
    //struct CFS_inode *raw_inode = NULL;
    struct CFS_inode *raw_inode = (struct CFS_inode *)kmalloc(sizeof(struct CFS_inode),GFP_KERNEL);
    CFS_get_inode(inode->i_sb, inode->i_ino, raw_inode);
    if (!raw_inode)
        return -EFAULT;
    raw_inode->mode = inode->i_mode;
    raw_inode->i_uid = fs_high2lowuid(i_uid_read(inode));
    raw_inode->i_gid = fs_high2lowgid(i_gid_read(inode));
    raw_inode->i_nlink = inode->i_nlink;
    raw_inode->file_size = inode->i_size;

    raw_inode->i_atime = (inode->i_atime.tv_sec);
    raw_inode->i_mtime = (inode->i_mtime.tv_sec);
    raw_inode->i_ctime = (inode->i_ctime.tv_sec);

    mark_buffer_dirty(bh);
    brelse(bh);
    return 0;
}


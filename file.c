#include "CFS.h"


int CFS_iterate(struct file *filp, struct dir_context *ctx)
{
	struct CFS_inode H_inode;
	struct super_block *sb = filp->f_inode->i_sb;

	printk(KERN_INFO "CFS: Iterate on inode [%llu]\n",
		   filp->f_inode->i_ino);

	if (-1 == CFS_get_inode(sb, filp->f_inode->i_ino, &H_inode))
		return -EFAULT;

	printk(KERN_INFO "H_inode.dir_children_count is %llu\n",
		   H_inode.dir_children_count);
	if (ctx->pos >= H_inode.dir_children_count)
	{
		return 0;
	}

	if (H_inode.blocks == 0)
	{
		printk(KERN_INFO
			   "CFS: inode [%lu] has no data!\n",
			   filp->f_inode->i_ino);
		return 0;
	}

	uint64_t i, dir_unread;
	dir_unread = H_inode.dir_children_count;
	printk(KERN_INFO "CFS: dir_unread [%llu]\n", dir_unread);
	if (dir_unread == 0)
	{
		return 0;
	}

	struct CFS_dir_record *dir_arr =
		kmalloc(sizeof(struct CFS_dir_record) * dir_unread, GFP_KERNEL);

	struct buffer_head *bh;
	for (i = 0; (i < H_inode.blocks) && (dir_unread > 0); ++i)
	{
		bh = sb_bread(sb, H_inode.block[i]);
		uint64_t len = dir_unread * sizeof(struct CFS_dir_record);
		uint64_t off = H_inode.dir_children_count - dir_unread;
		if (len < CFS_BLOCKSIZE)
		{ //read over
			memcpy(dir_arr + (off * sizeof(struct CFS_dir_record)),
				   bh->b_data, len);
			dir_unread = 0;
		}
		else
		{
			memcpy(dir_arr + (off * sizeof(struct CFS_dir_record)),
				   bh->b_data, CFS_BLOCKSIZE);
			dir_unread -=
				CFS_BLOCKSIZE / sizeof(struct CFS_dir_record);
		}
		brelse(bh);
	}
	for (i = 0; i < H_inode.dir_children_count; ++i)
	{
		printk(KERN_INFO " dir_arr[i].filename is %s\n",
			   dir_arr[i].filename);
		dir_emit(ctx, dir_arr[i].filename, strlen(dir_arr[i].filename),
				 dir_arr[i].inode_no, DT_REG);
		ctx->pos++;
	}
	kfree(dir_arr);
	printk(KERN_INFO "ctx->pos is %llu\n", ctx->pos);
	return 0;
}

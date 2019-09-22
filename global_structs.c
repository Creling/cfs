#include "CFS.h"


/* 注册文件系统 */
struct file_system_type CFS_type = {
	.owner = THIS_MODULE,
	.name = "CFS",
	.mount = CFS_mount,
	.kill_sb = CFS_kill_superblock,	
};

struct file_operations CFS_file_ops = {
	.owner = THIS_MODULE,
	.llseek = generic_file_llseek,
	.mmap = generic_file_mmap,
	.fsync = generic_file_fsync,
	.read_iter = generic_file_read_iter,
	.write_iter = generic_file_write_iter,
};

struct file_operations CFS_dir_ops = {
	.owner = THIS_MODULE,
	.iterate = CFS_iterate, // ls命令的前置
};

struct inode_operations CFS_inode_ops = {
	.lookup = CFS_lookup,  // ls命令的前置
	.mkdir = CFS_mkdir, // 创建文件夹
    .create = CFS_create, // 创建文件
    .unlink = CFS_unlink, // 删除
};


struct super_operations CFS_super_ops = {
    .evict_inode = CFS_evict_inode,  // 清空inode
    .write_inode = CFS_write_inode,  // 初始化inode
};

struct address_space_operations CFS_aops = {
	.readpage = CFS_readpage,
    .writepage = CFS_writepage,
	.write_begin = CFS_write_begin,
	.write_end = generic_write_end,
};
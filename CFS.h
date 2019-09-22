#ifndef CFS_H
#define CFS_H

#ifndef MKFS

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>


#endif

#define setbit(number, x) number |= 1UL << x
#define clearbit(number, x) number &= ~(1UL << x)

/* 常量区 */
#define magic_number_NUM 57117239
#define CFS_BLOCKSIZE 4096
#define CFS_FILE_MAX_BLOCKS 10
#define CFS_ROOT_INODE_NUM 0
#define CFS_FILENAME_MAX_LEN 256
#define RESERVE_BLOCKS 2 //dummy and sb


struct CFS_super_block
{
	uint64_t version;
	uint64_t magic_number; // 标识一个文件系统
	uint64_t block_size;   // 每个block的大小
	uint64_t inodes_count;
	uint64_t free_blocks;
	uint64_t blocks_count;
	uint64_t bmap_block_index;
	uint64_t imap_block_index;
	uint64_t inode_table_block_index;
	uint64_t data_block_index;
	char padding[4016];
};

struct CFS_inode
{
	mode_t mode; //sizeof(mode_t) is 4
	uint64_t inode_no;
	uint64_t blocks;
	uint64_t block[CFS_FILE_MAX_BLOCKS];
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
	int32_t i_uid;
	int32_t i_gid;
	int32_t i_nlink;
	int64_t i_atime;
	int64_t i_mtime;
	int64_t i_ctime;
	char padding[112];
};

struct CFS_dir_record
{
	char filename[CFS_FILENAME_MAX_LEN];
	uint64_t inode_no;
};

#ifndef MKFS

/* super_operations */
int CFS_write_inode(struct inode *inode, struct writeback_control *wbc);
void CFS_evict_inode(struct inode *vfs_inode);

/* address_space_operations */
int CFS_readpage(struct file *file, struct page *page);
int CFS_writepage(struct page *page, struct writeback_control *wbc);
int CFS_write_begin(struct file *file, struct address_space *mapping, loff_t pos,
					unsigned len, unsigned flags, struct page **pagep, void **fsdata);

/* inode_operations */ 

int CFS_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int CFS_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
int CFS_unlink(struct inode *dir, struct dentry *dentry);
struct dentry* CFS_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);

/* file_operations */ 
int CFS_iterate(struct file *filp, struct dir_context *ctx);

/* tools */
int checkbit(uint8_t number, int x);
int CFS_find_first_zero_bit(const void *vaddr, unsigned size);
int get_bmap(struct super_block *sb, uint8_t *bmap, ssize_t bmap_size);
int get_imap(struct super_block *sb, uint8_t *imap, ssize_t imap_size);
uint64_t CFS_get_empty_block(struct super_block *sb);
uint64_t CFS_get_empty_inode(struct super_block *sb);
int save_bmap(struct super_block *sb, uint8_t *bmap, ssize_t bmap_size);
int set_and_save_imap(struct super_block *sb, uint64_t inode_num, uint8_t value);
int set_and_save_bmap(struct super_block *sb, uint64_t block_num, uint8_t value);
int CFS_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode);

//block oprations
int save_block(struct super_block *sb, uint64_t block_num, void *buf, ssize_t size);
int CFS_get_block(struct inode *inode, sector_t block,
				  struct buffer_head *bh, int create);
int alloc_block_for_inode(struct super_block *sb, struct CFS_inode *p_H_inode, ssize_t size);

//inode oprerations
ssize_t CFS_read_inode_data(struct inode *inode, void *buf, size_t size);
ssize_t CFS_write_inode_data(struct inode *inode, const void *buf, size_t count);
int save_inode(struct super_block *sb, struct CFS_inode H_inode);
int CFS_get_inode(struct super_block *sb, uint64_t inode_no, struct CFS_inode *raw_inode);



//dir operations
int CFS_iterate(struct file *filp, struct dir_context *ctx);
int CFS_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int CFS_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
int CFS_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode);
int CFS_unlink(struct inode *dir, struct dentry *dentry);
struct dentry* CFS_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);

//super_block operations
int save_super(struct super_block *sb);
int CFS_fill_super(struct super_block *sb, void *data, int silent);

void CFS_convert_inode(struct CFS_inode *H_inode, struct inode *vfs_inode);

//file-system operations
struct dentry *CFS_mount(struct file_system_type *fs_type, int flags,
						 const char *dev_name, void *data);
void CFS_kill_superblock(struct super_block *s);
#endif 
#endif

/*
 * CFS disk layout:
 * 100MB disk -> 25600 blocks
 * And can write 25600 files at most.
 * inode size is 128B
 * block size is 4096B <=> 4K
 * block0 |dummy block
 * block1 |super block
 * block2 |bmap block
 * block3 |imap block
 * block4 - block(25600/(4096/64) + 3) |inode table
 * other blocks |data blocks
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <endian.h>
#include <linux/stat.h>


#define MKFS
#include "CFS.h"

static uint8_t *bmap;
static uint8_t *imap;
static uint64_t disk_size;
static uint64_t bmap_size;
static uint64_t imap_size;
static uint64_t inode_table_size;

static off_t get_file_size(const char *path)
{
	off_t ret = -1;
	struct stat statbuf;
	if (stat(path, &statbuf) < 0)
	{
		return ret;
	}
	return statbuf.st_size;
}
static int set_bmap(uint64_t idx, int value)
{
	if (!bmap)
	{
		return -1;
	}
	uint64_t array_idx = idx / (sizeof(char) * 8);
	uint64_t off = idx % (sizeof(char) * 8);
	if (array_idx > bmap_size * CFS_BLOCKSIZE)
	{
		printf("Set bmap error and idx is %llu\n", idx);
		return -1;
	}
	if (value)
		bmap[array_idx] |= (1 << off);
	else
		bmap[array_idx] &= ~(1 << off);
	return 0;
}
static int init_disk(int fd, const char *path)
{
	disk_size = get_file_size(path);
	if (disk_size == -1)
	{
		perror("Error: can not get disk size!\n");
		return -1;
	}
	printf("Disk size id %lu\n", disk_size);
	super_block.version = 1;
	super_block.block_size = CFS_BLOCKSIZE;
	super_block.magic_number = magic_number_NUM;
	super_block.blocks_count = disk_size / CFS_BLOCKSIZE;
	printf("blocks count is %llu\n", super_block.blocks_count);
	super_block.inodes_count = super_block.blocks_count;
	super_block.free_blocks = 0;
	//计算bmap
	bmap_size = super_block.blocks_count / (8 * CFS_BLOCKSIZE);
	super_block.bmap_block_index = RESERVE_BLOCKS;

	if (super_block.blocks_count % (8 * CFS_BLOCKSIZE) != 0)
	{
		bmap_size += 1;
	}
	bmap = (uint8_t *)malloc(bmap_size * CFS_BLOCKSIZE);
	memset(bmap, 0, bmap_size * CFS_BLOCKSIZE);

	//计算imap
	imap_size = super_block.inodes_count / (8 * CFS_BLOCKSIZE);
	super_block.imap_block_index = super_block.bmap_block_index + bmap_size;

	if (super_block.inodes_count % (8 * CFS_BLOCKSIZE) != 0)
	{
		imap_size += 1;
	}
	imap = (uint8_t *)malloc(imap_size * CFS_BLOCKSIZE);
	memset(imap, 0, imap_size * CFS_BLOCKSIZE);

	//计算inode_table
	inode_table_size = super_block.inodes_count / (CFS_BLOCKSIZE / (sizeof(struct CFS_inode)));
	super_block.inode_table_block_index = super_block.imap_block_index + imap_size;
	super_block.data_block_index = RESERVE_BLOCKS + bmap_size + imap_size + inode_table_size;
	super_block.free_blocks = super_block.blocks_count - super_block.data_block_index - 1;

	//设置bmap以及imap
	int idx;
	// plus one becase of the root dir
	for (idx = 0; idx < super_block.data_block_index + 1; ++idx)
	{
		if (set_bmap(idx, 1))
		{
			return -1;
		}
	}

	return 0;
}
static int write_sb(int fd)
{
	ssize_t ret;
	ret = write(fd, &super_block, sizeof(super_block));
	if (ret != CFS_BLOCKSIZE)
	{
		perror("Write super block error!\n");
		return -1;
	}
	printf("Super block written succesfully!\n");
	return 0;
}

static int write_bmap(int fd)
{
	ssize_t ret = -1;

	ret = write(fd, bmap, bmap_size * CFS_BLOCKSIZE);
	if (ret != bmap_size * CFS_BLOCKSIZE)
	{
		perror("Write_bmap() error!\n");
		return -1;
	}
	return 0;
}
static int write_imap(int fd)
{
	memset(imap, 0, imap_size * CFS_BLOCKSIZE);
	imap[0] |= 0x3;

	ssize_t res = write(fd, imap, imap_size * CFS_BLOCKSIZE);
	if (res != imap_size * CFS_BLOCKSIZE)
	{
		perror("write_imap() erroe!");
		return -1;
	}
	return 0;
}

static int write_itable(int fd)
{
	uint32_t _uid = getuid();
	uint32_t _gid = getgid();

	ssize_t ret;
	struct CFS_inode root_dir_inode;
	root_dir_inode.mode = S_IFDIR;
	root_dir_inode.inode_no = CFS_ROOT_INODE_NUM;
	root_dir_inode.blocks = 1;
	root_dir_inode.block[0] = super_block.data_block_index;
	root_dir_inode.dir_children_count = 3;
	root_dir_inode.i_gid = _gid;
	root_dir_inode.i_uid = _uid;
	root_dir_inode.i_nlink = 2;
	root_dir_inode.i_atime = root_dir_inode.i_mtime = root_dir_inode.i_ctime = ((int64_t)time(NULL));

	ret = write(fd, &root_dir_inode, sizeof(root_dir_inode));
	if (ret != sizeof(root_dir_inode))
	{
		perror("write_itable error!\n");
		return -1;
	}
	struct CFS_inode onefile_inode;
	onefile_inode.mode = S_IFREG;
	onefile_inode.inode_no = 1;
	onefile_inode.blocks = 0;
	onefile_inode.block[0] = 0;
	onefile_inode.file_size = 0;
	onefile_inode.i_gid = _gid;
	onefile_inode.i_uid = _uid;
	onefile_inode.i_nlink = 1;
	onefile_inode.i_atime = onefile_inode.i_mtime = onefile_inode.i_ctime = ((int64_t)time(NULL));

	ret = write(fd, &onefile_inode, sizeof(onefile_inode));
	if (ret != sizeof(onefile_inode))
	{
		perror("write_itable error!\n");
		return -1;
	}

	struct CFS_dir_record root_dir_c;
	const char *cur_dir = ".";
	const char *parent_dir = "..";

	memcpy(root_dir_c.filename, cur_dir, strlen(cur_dir) + 1);
	root_dir_c.inode_no = CFS_ROOT_INODE_NUM;
	struct CFS_dir_record root_dir_p;
	memcpy(root_dir_p.filename, parent_dir, strlen(parent_dir) + 1);
	root_dir_p.inode_no = CFS_ROOT_INODE_NUM;

	struct CFS_dir_record file_record;
	const char *onefile = "file";
	memcpy(file_record.filename, onefile, strlen(onefile) + 1);
	file_record.inode_no = 1;

	off_t current_off = lseek(fd, 0L, SEEK_CUR);
	printf("Current seek is %lu and rootdir at %lu\n", current_off, super_block.data_block_index * CFS_BLOCKSIZE);

	if (-1 == lseek(fd, super_block.data_block_index * CFS_BLOCKSIZE, SEEK_SET))
	{
		perror("lseek error\n");
		return -1;
	}
	ret = write(fd, &root_dir_c, sizeof(root_dir_c));
	ret = write(fd, &root_dir_p, sizeof(root_dir_p));
	ret = write(fd, &file_record, sizeof(file_record));
	if (ret != sizeof(root_dir_c))
	{
		perror("Write error!\n");
		return -1;
	}
	printf("Create root dir successfully!\n");
	return 0;
}
static int write_dummy(int fd)
{
	char dummy[CFS_BLOCKSIZE] = {0};
	ssize_t res = write(fd, dummy, CFS_BLOCKSIZE);
	if (res != CFS_BLOCKSIZE)
	{
		perror("write_dummy error!");
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int fd;
	ssize_t ret;

	if (argc != 2)
	{
		printf("Usage: mkfs <device>\n");
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1)
	{
		perror("Error opening the device");
		return -1;
	}
	ret = 1;

	init_disk(fd, argv[1]);
	write_dummy(fd);
	write_sb(fd);
	write_bmap(fd);
	write_imap(fd);
	write_itable(fd);
	//	write_root_dir(fd);
	//	write_welcome_file(fd);

	close(fd);
	return ret;
}

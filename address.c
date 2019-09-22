#include "CFS.h"

int CFS_readpage(struct file *file, struct page *page)
{
	printk(KERN_ERR "HUST: readpage");
	return block_read_full_page(page, CFS_get_block);
}

int CFS_writepage(struct page* page, struct writeback_control* wbc) {
	printk(KERN_ERR "HUST: in write page\n");
       return block_write_full_page(page, CFS_get_block, wbc);
}

int CFS_write_begin(struct file* file, struct address_space* mapping, 
		loff_t pos, unsigned len, unsigned flags, 
		struct page** pagep, void** fsdata) {
    int ret;
	printk(KERN_INFO "HUST: in write_begin\n");
    ret = block_write_begin(mapping, pos, len, flags, pagep, CFS_get_block);
    if (unlikely(ret))
        printk(KERN_INFO "HUST: Write failed for pos [%llu], len [%u]\n", pos, len);
    return ret;
}
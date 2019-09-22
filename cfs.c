#include "CFS.h"

extern struct file_system_type CFS_type;

struct dentry *CFS_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    struct dentry *ret;

    ret = mount_bdev(fs_type, flags, dev_name, data, CFS_fill_super);
    if (IS_ERR(ret))
        printk(KERN_ERR
               "Error: Fail to  mount CFS.\n");
    else
        printk(KERN_INFO
               "Info: Success to mount CFS on [%s].\n",
               dev_name);

    return ret;
}

void CFS_kill_superblock(struct super_block *s)
{
    kill_block_super(s);
    printk(KERN_INFO "Info: Success to umount CFS on.\n");
}

/* 文件系统初始化 */
int CFS_init(void)
{
    int ret;
    ret = register_filesystem(&CFS_type);

    return ret;
}

void CFS_exit(void)
{
    int ret;

    ret = unregister_filesystem(&CFS_type);
    if (ret == 0)
        printk(KERN_INFO "Info: Sucessfully unregistered CFS\n");
    else
        printk(KERN_ERR "Error: Failed to unregister CFS. Error: [%d]\n", ret);
}

module_init(CFS_init);
module_exit(CFS_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("zsc");
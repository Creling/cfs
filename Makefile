obj-m := CFS_fs.o
CFS_fs-objs := cfs.o inode.o map.o file.o super.o global_structs.o utility_functions.o address.o

all: drive mkfs

drive:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
mkfs_SOURCES:
	mkfs.c
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	

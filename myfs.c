
// [rootblock | log | free bitmap | inodes | data ]
// data - [rootblock | log | free bitmap | inodes | data]

struct file {
  int offset;
  struct inode *iptr;
  struct file_ops *ops;
};

struct inode {
  int size;
  int refcount; 
};

struct file_ops {
  int write;
  int read;
};

void sys_mount(char *dir, char *fstype, int nblocks)
{
  // mark blocks as used
  // alloc fs structures
}

void sys_unmount(char *dir)
{
    
}

void sys_write(int fd, char *buf, int len) {
  // find file table entry
  // file->ops->write
}


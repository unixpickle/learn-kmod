#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/statfs.h>
#include "virt_fs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("An in-memory virtual filesystem.");
MODULE_VERSION("0.01");

struct virt_fs_state {
  struct vfsmount* mnt;

  struct super_block* sb;
  struct dentry* root_dentry;
  struct inode* root_inode;
  struct dentry* file_dentry;
  struct inode* file_inode;

  struct virt_fs_node* root_node;
};

static struct virt_fs_state state;

// Root directory operations

int virt_fs_root_open(struct inode* inode, struct file* file) {
  printk(KERN_INFO "virt_fs: root_open\n");
  return 0;
}

int virt_fs_root_iterate(struct file* file, struct dir_context* ctx) {
  printk(KERN_INFO "virt_fs: root_iterate(%lld)\n", ctx->pos);
  dir_emit_dots(file, ctx);
  if (ctx->pos == 2) {
    printk(KERN_INFO "virt_fs: serving up file entry\n");
    ctx->actor(ctx, "file.txt", 8, ctx->pos, state.file_inode->i_ino,
               DT_REG | 0777);
    ctx->pos = 3;
  }
  return 0;
}

static struct file_operations root_fops = {
    .open = virt_fs_root_open,
    .iterate = virt_fs_root_iterate,
};

struct dentry* virt_fs_root_lookup(struct inode* inode,
                                   struct dentry* entry,
                                   unsigned int flags) {
  printk(KERN_INFO "virt_fs: lookup %s\n", entry->d_name.name);
  if (!strcmp(entry->d_name.name, "file.txt")) {
    d_add(entry, state.file_inode);
    return entry;
  }
  return ERR_PTR(-ENOENT);
}

static struct inode_operations root_iops = {
    .lookup = virt_fs_root_lookup,
};

// File operations

int virt_fs_file_open(struct inode* inode, struct file* file) {
  printk(KERN_INFO "virt_fs: file_open\n");
  return -EPERM;
}

static struct file_operations file_fops = {
    .open = virt_fs_file_open,
};

static const struct qstr file_name = {
    .hash_len = 8,
    .name = "file.txt",
};

// Super block

static struct file_system_type fs_type;

int virt_fs_statfs(struct dentry* entry, struct kstatfs* stats) {
  stats->f_bsize = 512;
  stats->f_blocks = 1L << 33;
  stats->f_bfree = 1L << 32;
  stats->f_bavail = 1L << 32;
  stats->f_files = 0;
  stats->f_ffree = 1L << 41;
  return 0;
}

static struct super_operations super_ops = {
    .statfs = virt_fs_statfs,
};

static int virt_fs_fill_super(struct super_block* sb, void* data, int flags) {
  int res;

  if (state.sb) {
    return -EINVAL;
  }
  state.sb = sb;

  sb->s_blocksize_bits = 9;
  sb->s_blocksize = 512;
  sb->s_maxbytes = ((u64)1) << 32;
  sb->s_type = &fs_type;
  sb->s_op = &super_ops;

  state.root_inode = new_inode(state.sb);
  if (!state.root_inode) {
    res = -ENOMEM;
    goto fail_sb;
  }
  state.root_inode->i_mode = S_IFDIR | 0777;
  state.root_inode->i_opflags = IOP_LOOKUP;
  state.root_inode->i_fop = &root_fops;
  state.root_inode->i_op = &root_iops;
  state.root_inode->i_ino = 0;

  state.root_dentry = d_make_root(state.root_inode);
  if (IS_ERR(state.root_dentry)) {
    res = PTR_ERR(state.root_dentry);
    goto fail_sb;
  }
  printk(KERN_INFO "virt_fs: root inode=%ld\n", state.root_inode->i_ino);

  sb->s_root = state.root_dentry;

  state.file_inode = new_inode(state.sb);
  if (!state.file_inode) {
    res = -ENOMEM;
    goto fail_root_dentry;
  }
  state.file_inode->i_mode = S_IFREG | 0777;
  state.file_inode->i_fop = &file_fops;
  state.file_inode->i_size = 11;
  state.file_inode->i_ino = 1;

  state.file_dentry = d_alloc(state.root_dentry, &file_name);
  if (IS_ERR(state.file_dentry)) {
    iput(state.file_inode);
    res = PTR_ERR(state.file_dentry);
    goto fail_root_dentry;
  }
  d_instantiate(state.file_dentry, state.file_inode);

  return 0;

fail_root_dentry:
  dput(state.root_dentry);
fail_sb:
  deactivate_locked_super(state.sb);
  return res;
}

// File system type

static struct dentry* virt_fs_mount(struct file_system_type* type,
                                    int flags,
                                    const char* dev_name,
                                    void* data) {
  return mount_single(type, flags, NULL, virt_fs_fill_super);
}

static void virt_fs_kill_sb(struct super_block* sb) {
  dput(state.root_dentry);
  dput(state.file_dentry);
}

static struct file_system_type fs_type = {
    .name = "virt_fs",
    .mount = virt_fs_mount,
    .kill_sb = virt_fs_kill_sb,
    .owner = THIS_MODULE,
};

// Module lifecycle

static int __init virt_fs_init(void) {
  state.root_node = virt_fs_read_tar();
  if (!state.root_node) {
    return -EINVAL;
  }
  int res = register_filesystem(&fs_type);
  if (res) {
    virt_fs_free_tar(state.root_node);
    return res;
  }
  return 0;
}

static void __exit virt_fs_exit(void) {
  unregister_filesystem(&fs_type);
  virt_fs_free_tar(state.root_node);
}

module_init(virt_fs_init);
module_exit(virt_fs_exit);
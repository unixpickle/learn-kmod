#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/statfs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("An in-memory virtual filesystem.");
MODULE_VERSION("0.01");

struct virt_fs_state {
  struct vfsmount* mnt;

  struct super_block sb;
  struct dentry* root_dentry;
  struct inode* root_inode;
};

static struct virt_fs_state state;

// File system type

static struct dentry* virt_fs_mount(struct file_system_type* f,
                                    int flags,
                                    const char* dev_name,
                                    void* data) {
  int res;

  if (state.root_dentry) {
    return ERR_PTR(-EINVAL);
  }
  state.root_inode = new_inode(&state.sb);
  if (!state.root_inode) {
    return ERR_PTR(-ENOMEM);
  }

  state.root_dentry = d_make_root(state.root_inode);
  if (IS_ERR(state.root_dentry)) {
    res = PTR_ERR(state.root_dentry);
    goto fail_inode;
  }

  return state.root_dentry;

fail_inode:
  iput(state.root_inode);
  return ERR_PTR(res);
}

static void virt_fs_kill_sb(struct super_block* sb) {
  iput(state.root_inode);
  dput(state.root_dentry);
}

static struct file_system_type fs_type = {
    .name = "virt_fs",
    .mount = virt_fs_mount,
    .kill_sb = virt_fs_kill_sb,
    .owner = THIS_MODULE,
};

// Super block

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

static void setup_super_block(void) {
  state.sb.s_blocksize_bits = 9;
  state.sb.s_blocksize = 512;
  state.sb.s_maxbytes = ((u64)1) << 32;
  state.sb.s_type = &fs_type;
  state.sb.s_op = &super_ops;
}

// Module lifecycle

static int __init virt_fs_init(void) {
  int res = register_filesystem(&fs_type);
  if (res) {
    return res;
  }
  setup_super_block();
  return 0;
}

static void __exit virt_fs_exit(void) {
  unregister_filesystem(&fs_type);
}

module_init(virt_fs_init);
module_exit(virt_fs_exit);
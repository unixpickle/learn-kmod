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
  struct virt_fs_node* root_node;
};

static struct virt_fs_state state;

static struct inode* inode_for_node(struct virt_fs_node* node);
static struct virt_fs_node* node_for_inode(struct inode* inode);

// File operations

int virt_fs_open(struct inode* inode, struct file* file) {
  printk(KERN_INFO "virt_fs: open\n");
  file->private_data = inode;
  return 0;
}

int virt_fs_iterate(struct file* file, struct dir_context* ctx) {
  struct virt_fs_node* node = node_for_inode(file->private_data);
  printk(KERN_INFO "virt_fs: iterate(%lld)\n", ctx->pos);
  dir_emit_dots(file, ctx);
  if (ctx->pos >= 2 && ctx->pos - 2 < node->num_children) {
    struct virt_fs_node* child = node->children[ctx->pos - 2];
    struct inode* inode = inode_for_node(child);
    int name_len = strlen(child->base_name);
    if (child->base_name[name_len - 1] == '/') {
      name_len--;
    }
    ctx->actor(ctx, child->base_name, name_len, ctx->pos, inode->i_ino,
               inode->i_mode);
    ctx->pos++;
  }
  return 0;
}

static struct file_operations virt_fs_fops = {
    .open = virt_fs_open,
    .iterate = virt_fs_iterate,
};

struct dentry* virt_fs_lookup(struct inode* inode,
                              struct dentry* entry,
                              unsigned int flags) {
  struct virt_fs_node* node = node_for_inode(inode);
  int i;
  for (i = 0; i < node->num_children; ++i) {
    if (!memcmp(node->children[i]->base_name, entry->d_name.name,
                entry->d_name.len)) {
      char final_char = node->children[i]->base_name[entry->d_name.len];
      if (final_char == '/' || final_char == 0) {
        d_add(entry, inode_for_node(node->children[i]));
      }
      break;
    }
  }
  return entry;
}

static struct inode_operations virt_fs_iops = {
    .lookup = virt_fs_lookup,
};

// Inodes and dentries.

static struct inode* inode_for_node(struct virt_fs_node* node) {
  struct inode* inode = iget_locked(state.sb, (unsigned long)node);
  if (inode->i_state & I_NEW) {
    inode->i_mode = 0777;
    if (!node->file_data) {
      inode->i_mode |= S_IFDIR;
      inode->i_opflags = IOP_LOOKUP;
    }
    inode->i_fop = &virt_fs_fops;
    inode->i_op = &virt_fs_iops;
    inode->i_flags = 0;
    unlock_new_inode(inode);
  }
  return inode;
}

static struct virt_fs_node* node_for_inode(struct inode* inode) {
  return (struct virt_fs_node*)inode->i_ino;
}

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
  struct inode* root_inode;

  if (state.sb) {
    return -EINVAL;
  }
  state.sb = sb;

  sb->s_blocksize_bits = 9;
  sb->s_blocksize = 512;
  sb->s_maxbytes = ((u64)1) << 32;
  sb->s_type = &fs_type;
  sb->s_op = &super_ops;

  root_inode = inode_for_node(state.root_node);
  printk(KERN_INFO "root_inode mode %d\n", root_inode->i_mode);

  sb->s_root = d_make_root(root_inode);
  if (IS_ERR(sb->s_root)) {
    return PTR_ERR(sb->s_root);
  }

  return 0;
}

// File system type

static struct dentry* virt_fs_mount(struct file_system_type* type,
                                    int flags,
                                    const char* dev_name,
                                    void* data) {
  return mount_single(type, flags, NULL, virt_fs_fill_super);
}

static void virt_fs_kill_sb(struct super_block* sb) {}

static struct file_system_type fs_type = {
    .name = "virt_fs",
    .mount = virt_fs_mount,
    .kill_sb = virt_fs_kill_sb,
    .owner = THIS_MODULE,
};

// Module lifecycle

static int __init virt_fs_init(void) {
  int res;
  state.root_node = virt_fs_read_tar();
  if (!state.root_node) {
    return -EINVAL;
  }
  res = register_filesystem(&fs_type);
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
#ifndef __VIRT_FS_H__
#define __VIRT_FS_H__

#include <linux/fs.h>
#include <linux/list.h>

// virt_fs_tar.c

struct virt_fs_node {
  // E.g. "photos/file.jpg"
  const char* full_name;

  // E.g. "file.jpg"
  const char* base_name;

  // Is NULL if this is a directory.
  const char* file_data;
  int file_size;

  struct list_head parent_link;
  struct list_head children;
};

struct virt_fs_node* virt_fs_read_tar(void);
void virt_fs_free_tar(struct virt_fs_node* node);

#endif
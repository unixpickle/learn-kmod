#include <linux/gfp.h>
#include <linux/slab.h>
#include "virt_fs.h"

extern const char* virt_fs_data;
extern const int virt_fs_data_size;

static const char* get_basename(const char* name) {
  int last_slash = -1;
  int i;
  for (i = 0; i < strlen(name); ++i) {
    if (name[i] == '/') {
      last_slash = i;
    }
  }
  return name + last_slash + 1;
}

static const char* get_dirname(const char* name) {
  static char res[256];
  int last_slash = -1;
  int i;
  for (i = 0; i < strlen(name) - 1 && i + 1 < sizeof(res); ++i) {
    if (name[i] == '/') {
      last_slash = i;
    }
    res[i] = name[i];
  }
  res[last_slash + 1] = 0;
  return res;
}

static struct virt_fs_node* find_node(struct virt_fs_node* parent,
                                      const char* name) {
  int i;
  if (!strcmp(parent->full_name, name)) {
    return parent;
  }
  if (!parent->file_data) {
    for (i = 0; i < parent->num_children; ++i) {
      struct virt_fs_node* node = find_node(parent->children[i], name);
      if (node) {
        return node;
      }
    }
  }
  return NULL;
}

static void insert_child(struct virt_fs_node* parent,
                         struct virt_fs_node* child) {
  struct virt_fs_node** new_children = kmalloc(
      sizeof(struct virt_fs_node*) * (parent->num_children + 1), GFP_KERNEL);
  int i;
  for (i = 0; i < parent->num_children; ++i) {
    new_children[i] = parent->children[i];
  }
  new_children[parent->num_children++] = child;
  if (parent->children) {
    kfree(parent->children);
  }
  parent->children = new_children;
}

struct virt_fs_node* virt_fs_read_tar() {
  int offset;
  struct virt_fs_node* root_node =
      kmalloc(sizeof(struct virt_fs_node), GFP_KERNEL);
  memset(root_node, 0, sizeof(*root_node));
  root_node->full_name = "";
  root_node->base_name = "";

  for (offset = 0; offset + 512 <= virt_fs_data_size; offset += 512) {
    const char* full_name = virt_fs_data + offset;
    if (!strlen(full_name)) {
      continue;
    }
    struct virt_fs_node* new_node;
    struct virt_fs_node* parent = find_node(root_node, get_dirname(full_name));
    if (!parent) {
      virt_fs_free_tar(root_node);
      return NULL;
    }
    new_node =
        (struct virt_fs_node*)kmalloc(sizeof(struct virt_fs_node), GFP_KERNEL);
    memset(new_node, 0, sizeof(*new_node));
    new_node->full_name = full_name;
    new_node->base_name = get_basename(new_node->full_name);
    if (strlen(new_node->base_name)) {
      char size_str[13];
      long file_size;
      memset(size_str, 0, 13);
      memcpy(size_str, virt_fs_data + offset + 124, 12);
      kstrtol(size_str, 8, &file_size);
      new_node->file_size = (int)file_size;
      new_node->file_data = virt_fs_data + offset + 512;
      offset += new_node->file_size;
      if (new_node->file_size % 512) {
        offset += 512 - (new_node->file_size % 512);
      }
    }
    insert_child(parent, new_node);
  }

  return root_node;
}

void virt_fs_free_tar(struct virt_fs_node* node) {
  int i;
  if (node->children) {
    for (i = 0; i < node->num_children; ++i) {
      virt_fs_free_tar(node->children[i]);
    }
    kfree(node->children);
  }
  kfree(node);
}

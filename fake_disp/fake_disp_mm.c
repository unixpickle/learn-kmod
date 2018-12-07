#include <linux/gfp.h>
#include "fake_disp.h"

struct fake_disp_page {
  struct page* page;
  struct list_head list;
};

struct fake_disp_gem_object {
  struct drm_gem_object base;
  struct list_head pages;
};

static void fake_disp_gem_free_pages(struct fake_disp_gem_object* obj) {
  struct list_head* next_page = obj->pages.next;
  while (next_page != &obj->pages) {
    struct fake_disp_page* page =
        list_entry(next_page, struct fake_disp_page, list);
    next_page = next_page->next;
    __free_page(page->page);
    kfree(page);
  }
  INIT_LIST_HEAD(&obj->pages);
}

static int fake_disp_gem_alloc_pages(struct fake_disp_gem_object* obj,
                                     size_t size) {
  size_t total = 0;
  while (total < size) {
    struct fake_disp_page* page;
    printk(KERN_INFO "fake_disp allocating page...\n");
    struct page* raw_page = alloc_page(0);
    printk(KERN_INFO "fake_disp allocated page!\n");
    if (!raw_page) {
      fake_disp_gem_free_pages(obj);
      return -ENOMEM;
    }
    page = kmalloc(sizeof(struct fake_disp_page), GFP_KERNEL);
    if (!page) {
      __free_page(raw_page);
      fake_disp_gem_free_pages(obj);
      return -ENOMEM;
    }
    page->page = raw_page;
    list_add(&page->list, &obj->pages);
    total += PAGE_SIZE;
  }
  return 0;
}

static struct drm_gem_object* fake_disp_gem_create(
    struct drm_device* dev,
    struct drm_mode_create_dumb* args) {
  struct fake_disp_gem_object* obj;
  int res;
  unsigned int min_pitch;

  min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
  if (args->pitch < min_pitch) {
    args->pitch = min_pitch;
  }
  if (args->size < args->pitch * args->height) {
    args->size = args->pitch * args->height;
  }
  if (args->size % PAGE_SIZE) {
    args->size += PAGE_SIZE - (args->size % PAGE_SIZE);
  }

  printk(KERN_INFO "fake_disp gem_create (size=%lld) (pid=%d)\n", args->size,
         task_pid_nr(current));

  obj = kmalloc(sizeof(struct fake_disp_gem_object), GFP_KERNEL);
  if (!obj) {
    return ERR_PTR(-ENOMEM);
  }

  res = drm_gem_object_init(dev, &obj->base, args->size);
  if (res) {
    goto fail_1;
  }

  res = drm_gem_create_mmap_offset(&obj->base);
  if (res) {
    goto fail_2;
  }

  res = fake_disp_gem_alloc_pages(obj, args->size);
  if (res) {
    goto fail_3;
  }

  return &obj->base;

fail_3:
  drm_gem_free_mmap_offset(&obj->base);
fail_2:
  drm_gem_object_release(&obj->base);
fail_1:
  kfree(obj);

  return ERR_PTR(res);
}

static int fake_disp_gem_create_handle(struct drm_file* file_priv,
                                       struct drm_device* dev,
                                       struct drm_mode_create_dumb* args,
                                       u32* handle) {
  int res;

  struct drm_gem_object* obj = fake_disp_gem_create(dev, args);
  if (IS_ERR(obj)) {
    return PTR_ERR(obj);
  }

  res = drm_gem_handle_create(file_priv, obj, handle);
  if (res) {
    fake_disp_gem_free_object(obj);
    return res;
  }

  drm_gem_object_put_unlocked(obj);
  return 0;
}

int fake_disp_mmap(struct file* filp, struct vm_area_struct* vma) {
  int res;
  size_t offset = 0;
  struct fake_disp_page* next_page;
  struct drm_gem_object* gem_obj;
  struct fake_disp_gem_object* obj;

  printk(KERN_INFO "fake_disp mmap(%lu) (pid=%d)\n", vma->vm_pgoff,
         task_pid_nr(current));

  res = drm_gem_mmap(filp, vma);
  if (res) {
    return res;
  }

  gem_obj = vma->vm_private_data;
  obj = container_of(gem_obj, struct fake_disp_gem_object, base);

  list_for_each_entry_reverse(next_page, &obj->pages, list) {
    res = vm_insert_page(vma, vma->vm_start + offset, next_page->page);
    if (res) {
      printk(KERN_WARNING "fake_disp remap_pfn_range() -> %d\n", res);
      drm_gem_vm_close(vma);
      return res;
    }
    offset += PAGE_SIZE;
  }

  return 0;
}

int fake_disp_gem_dumb_create(struct drm_file* file_priv,
                              struct drm_device* dev,
                              struct drm_mode_create_dumb* args) {
  printk(KERN_INFO "fake_disp gem_dumb_create (pid=%d)\n",
         task_pid_nr(current));
  return fake_disp_gem_create_handle(file_priv, dev, args, &args->handle);
}

int fake_disp_gem_dumb_map_offset(struct drm_file* file,
                                  struct drm_device* dev,
                                  uint32_t handle,
                                  uint64_t* offset) {
  int res = drm_gem_dumb_map_offset(file, dev, handle, offset);
  printk(KERN_INFO "fake_disp gem_dumb_mmap_offset() -> %llu\n", *offset);
  return res;
}

int fake_disp_gem_dumb_destroy(struct drm_file* file_priv,
                               struct drm_device* dev,
                               uint32_t handle) {
  printk(KERN_INFO "fake_disp gem_dumb_destroy\n");
  return drm_gem_dumb_destroy(file_priv, dev, handle);
}

void fake_disp_gem_free_object(struct drm_gem_object* gem_obj) {
  struct fake_disp_gem_object* obj =
      container_of(gem_obj, struct fake_disp_gem_object, base);
  fake_disp_gem_free_pages(obj);
  drm_gem_free_mmap_offset(gem_obj);
  drm_gem_object_release(gem_obj);
  kfree(obj);
}

struct drm_framebuffer* fake_disp_user_framebuffer_create(
    struct drm_device* dev,
    struct drm_file* filp,
    const struct drm_mode_fb_cmd2* mode_cmd) {
  struct drm_framebuffer* res;
  printk(KERN_INFO "fake_disp creating framebuffer (pid=%d)\n",
         task_pid_nr(current));
  res = drm_gem_fb_create(dev, filp, mode_cmd);
  if (IS_ERR(res)) {
    printk(KERN_INFO "fake_disp framebuffer create failed -> %ld (pid=%d)\n",
           PTR_ERR(res), task_pid_nr(current));
  } else {
    printk(KERN_INFO "fake_disp framebuffer create -> %p %dx%d (pid=%d)\n", res,
           res->width, res->height, task_pid_nr(current));
  }
  return res;
}

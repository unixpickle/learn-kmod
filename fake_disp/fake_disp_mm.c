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
  printk(KERN_INFO "fake_disp: gem_free_pages()\n");
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
    struct page* raw_page = alloc_page(GFP_HIGHUSER);
    if (!raw_page) {
      fake_disp_gem_free_pages(obj);
      return -ENOMEM;
    }
    page = kzalloc(sizeof(struct fake_disp_page), GFP_KERNEL);
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

  printk(KERN_INFO "fake_disp: gem_create (size=%lld) (pid=%d)\n", args->size,
         task_pid_nr(current));

  obj = kzalloc(sizeof(struct fake_disp_gem_object), GFP_KERNEL);
  if (!obj) {
    return ERR_PTR(-ENOMEM);
  }
  INIT_LIST_HEAD(&obj->pages);

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

  printk(KERN_INFO "fake_disp: mmap(%lu) (pid=%d)\n", vma->vm_pgoff,
         task_pid_nr(current));

  res = drm_gem_mmap(filp, vma);
  if (res) {
    return res;
  }

  // Allow us to use vm_insert_page().
  vma->vm_flags &= ~VM_PFNMAP;

  // This is fake, so let's disregard it.
  vma->vm_pgoff = 0;

  gem_obj = vma->vm_private_data;
  obj = container_of(gem_obj, struct fake_disp_gem_object, base);

  list_for_each_entry_reverse(next_page, &obj->pages, list) {
    res = vm_insert_page(vma, vma->vm_start + offset, next_page->page);
    if (res) {
      printk(KERN_WARNING "fake_disp: remap_pfn_range() -> %d\n", res);
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
  printk(KERN_INFO "fake_disp: gem_dumb_create (pid=%d)\n",
         task_pid_nr(current));
  return fake_disp_gem_create_handle(file_priv, dev, args, &args->handle);
}

int fake_disp_gem_dumb_map_offset(struct drm_file* file,
                                  struct drm_device* dev,
                                  uint32_t handle,
                                  uint64_t* offset) {
  int res = drm_gem_dumb_map_offset(file, dev, handle, offset);
  printk(KERN_INFO "fake_disp: gem_dumb_mmap_offset() -> %llu\n", *offset);
  return res;
}

int fake_disp_gem_dumb_destroy(struct drm_file* file_priv,
                               struct drm_device* dev,
                               uint32_t handle) {
  printk(KERN_INFO "fake_disp: gem_dumb_destroy\n");
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

static void fake_disp_user_framebuffer_destroy(struct drm_framebuffer* fb) {
  drm_framebuffer_cleanup(fb);
  kfree(fb);
}

static const struct drm_framebuffer_funcs fake_disp_fb_funcs = {
    .destroy = fake_disp_user_framebuffer_destroy,
};

struct drm_framebuffer* fake_disp_user_framebuffer_create(
    struct drm_device* dev,
    struct drm_file* filp,
    const struct drm_mode_fb_cmd2* mode_cmd) {
  struct drm_framebuffer* res;
  int err;

  printk(KERN_INFO "fake_disp: creating framebuffer (pid=%d)\n",
         task_pid_nr(current));

  res = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
  if (!res) {
    return ERR_PTR(-ENOMEM);
  }

  drm_helper_mode_fill_fb_struct(dev, res, mode_cmd);
  err = drm_framebuffer_init(dev, res, &fake_disp_fb_funcs);
  if (err) {
    printk(KERN_INFO "fake_disp: drm_framebuffer_init() -> %d\n", err);
    kfree(res);
    return ERR_PTR(err);
  }

  printk(KERN_INFO "fake_disp: created framebuffer of size %dx%d\n", res->width,
         res->height);

  return res;
}

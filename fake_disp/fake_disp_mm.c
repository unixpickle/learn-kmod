#include "fake_disp.h"

int fake_disp_mmap(struct file* filp, struct vm_area_struct* vma) {
  printk(KERN_INFO "fake_disp mmap(%lu) (pid=%d)\n", vma->vm_pgoff,
         task_pid_nr(current));
  return drm_gem_cma_mmap(filp, vma);
}

int fake_disp_gem_dumb_create(struct drm_file* file,
                              struct drm_device* dev,
                              struct drm_mode_create_dumb* args) {
  printk(KERN_INFO "fake_disp gem_dumb_create (pid=%d)\n",
         task_pid_nr(current));
  return drm_gem_cma_dumb_create(file, dev, args);
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
  return 0;
}

void fake_disp_gem_free_object(struct drm_gem_object* gem_obj) {
  printk(KERN_INFO "fake_disp gem_free_object\n");
  drm_gem_cma_free_object(gem_obj);
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

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_encoder.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("A fake external monitor.");
MODULE_VERSION("0.01");

#define WIDTH 1920
#define HEIGHT 1080

struct fake_disp_state {
  struct drm_device* device;
  struct drm_crtc crtc;
  struct drm_encoder encoder;
  struct drm_connector connector;
};

static struct fake_disp_state state;

// CRTC

static void fake_disp_crtc_dpms(struct drm_crtc* crtc, int mode) {
  printk(KERN_INFO "fake_disp crtc_dpms(%d)\n", mode);
}

static int fake_disp_crtc_mode_set_base(struct drm_crtc* crtc,
                                        int x,
                                        int y,
                                        struct drm_framebuffer* old_fb) {
  printk(KERN_INFO "fake_disp crtc_mode_set_base\n");
  return 0;
}

static int fake_disp_crtc_mode_set(struct drm_crtc* crtc,
                                   struct drm_display_mode* mode,
                                   struct drm_display_mode* adjusted_mode,
                                   int x,
                                   int y,
                                   struct drm_framebuffer* old_fb) {
  printk(KERN_INFO "fake_disp crtc_mode_set\n");
  return 0;
}

static void fake_disp_crtc_nop(struct drm_crtc* crtc) {
  printk(KERN_INFO "fake_disp crtc_nop\n");
}

static int fake_disp_crtc_page_flip(struct drm_crtc* crtc,
                                    struct drm_framebuffer* fb,
                                    struct drm_pending_vblank_event* event,
                                    uint32_t page_flip_flags,
                                    struct drm_modeset_acquire_ctx* ctx) {
  printk(KERN_INFO "fake_disp crtc_page_flip\n");
  unsigned long flags;
  crtc->primary->fb = fb;
  if (event) {
    spin_lock_irqsave(&state.device->event_lock, flags);
    drm_crtc_send_vblank_event(crtc, event);
    spin_unlock_irqrestore(&state.device->event_lock, flags);
  }
  return 0;
}

static const struct drm_crtc_funcs fake_disp_crtc_funcs = {
    .set_config = drm_crtc_helper_set_config,
    .destroy = drm_crtc_cleanup,
    .page_flip = fake_disp_crtc_page_flip,
};

static const struct drm_crtc_helper_funcs fake_disp_crtc_helper_funcs = {
    .dpms = fake_disp_crtc_dpms,
    .mode_set = fake_disp_crtc_mode_set,
    .mode_set_base = fake_disp_crtc_mode_set_base,
    .prepare = fake_disp_crtc_nop,
    .commit = fake_disp_crtc_nop,
};

// Connector

static int fake_disp_conn_get_modes(struct drm_connector* connector) {
  int res = drm_add_modes_noedid(connector, WIDTH, HEIGHT);
  printk(KERN_INFO "fake_disp conn_get_modes %d\n", res);
  // return res;
  return 1;
}

static enum drm_mode_status fake_disp_conn_mode_valid(
    struct drm_connector* connector,
    struct drm_display_mode* mode) {
  printk(KERN_INFO "fake_disp conn_mode_valid %d %d (pid=%d)\n", mode->hdisplay,
         mode->vdisplay, task_pid_nr(current));
  // if (mode->hdisplay != WIDTH || mode->vdisplay != HEIGHT) {
  //   return MODE_BAD;
  // }
  return MODE_OK;
}

static struct drm_encoder* fake_disp_conn_best_encoder(
    struct drm_connector* connector) {
  return &state.encoder;
}

static const struct drm_connector_helper_funcs fake_disp_conn_helper_funcs = {
    .get_modes = fake_disp_conn_get_modes,
    .mode_valid = fake_disp_conn_mode_valid,
    .best_encoder = fake_disp_conn_best_encoder,
};

static const struct drm_connector_funcs fake_disp_conn_funcs = {
    .dpms = drm_helper_connector_dpms,
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = drm_connector_cleanup,
};

// Encoder

static void fake_disp_enc_mode_set(struct drm_encoder* encoder,
                                   struct drm_display_mode* mode,
                                   struct drm_display_mode* adjusted_mode) {
  printk(KERN_INFO "fake_disp enc_mode_set\n");
}

static void fake_disp_enc_dpms(struct drm_encoder* encoder, int state) {
  printk(KERN_INFO "fake_disp enc_dpms\n");
}

static void fake_disp_enc_nop(struct drm_encoder* encoder) {
  printk(KERN_INFO "fake_disp enc_nop\n");
}

static const struct drm_encoder_helper_funcs fake_disp_enc_helper_funcs = {
    .dpms = fake_disp_enc_dpms,
    .mode_set = fake_disp_enc_mode_set,
    .prepare = fake_disp_enc_nop,
    .commit = fake_disp_enc_nop,
};

static const struct drm_encoder_funcs fake_disp_enc_funcs = {
    .destroy = drm_encoder_cleanup,
};

// Driver

int fake_disp_open(struct inode* inode, struct file* filp) {
  printk(KERN_INFO "fake_disp open() called (pid=%d).\n", task_pid_nr(current));
  return drm_open(inode, filp);
}

long fake_disp_ioctl(struct file* filp, unsigned int cmd, unsigned long arg) {
  long res = drm_ioctl(filp, cmd, arg);
  printk(KERN_INFO "fake_disp ioctl(%d) -> %ld\n", cmd, res);
  return res;
}

long fake_disp_compat_ioctl(struct file* filp,
                            unsigned int cmd,
                            unsigned long arg) {
  long res = drm_compat_ioctl(filp, cmd, arg);
  printk(KERN_INFO "fake_disp compat_ioctl(%d) -> %ld\n", cmd, res);
  return res;
}

int fake_disp_mmap(struct file* filp, struct vm_area_struct* vma) {
  printk(KERN_INFO "fake_disp mmap(%lu) (pid=%d)\n", vma->vm_pgoff,
         task_pid_nr(current));
  return drm_gem_cma_mmap(filp, vma);
}

static const struct file_operations fake_disp_fops = {
    .owner = THIS_MODULE,
    .open = fake_disp_open,
    .release = drm_release,
    .unlocked_ioctl = fake_disp_ioctl,
    .compat_ioctl = fake_disp_compat_ioctl,
    .poll = drm_poll,
    .read = drm_read,
    .llseek = noop_llseek,
    .mmap = fake_disp_mmap,
};

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

static struct drm_driver fake_disp_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET,
    .fops = &fake_disp_fops,
    .name = "fake_disp",
    .desc = "fake display interface",
    .date = "20181205",
    .major = 1,
    .minor = 0,
    .gem_vm_ops = &drm_gem_cma_vm_ops,
    .gem_free_object_unlocked = fake_disp_gem_free_object,
    .dumb_create = fake_disp_gem_dumb_create,
    .dumb_map_offset = fake_disp_gem_dumb_map_offset,
    .dumb_destroy = fake_disp_gem_dumb_destroy,
};

// Framebuffers

static struct drm_framebuffer* fake_disp_user_framebuffer_create(
    struct drm_device* dev,
    struct drm_file* filp,
    const struct drm_mode_fb_cmd2* mode_cmd) {
  printk(KERN_INFO "fake_disp creating framebuffer (pid=%d)\n",
         task_pid_nr(current));
  struct drm_framebuffer* res = drm_gem_fb_create(dev, filp, mode_cmd);
  if (IS_ERR(res)) {
    printk(KERN_INFO "fake_disp framebuffer create failed -> %d (pid=%d)\n",
           PTR_ERR(res), task_pid_nr(current));
  } else {
    printk(KERN_INFO "fake_disp framebuffer create -> %p %dx%d (pid=%d)\n", res,
           res->width, res->height, task_pid_nr(current));
  }
  return res;
}

const struct drm_mode_config_funcs bs_funcs = {
    .fb_create = fake_disp_user_framebuffer_create,
};

// Module lifecycle

static int __init fake_disp_init(void) {
  int res;

  state.device = drm_dev_alloc(&fake_disp_driver, NULL);
  if (IS_ERR(state.device)) {
    return PTR_ERR(state.device);
  }

  drm_mode_config_init(state.device);
  state.device->mode_config.min_width = WIDTH;
  state.device->mode_config.min_height = HEIGHT;
  state.device->mode_config.max_width = WIDTH;
  state.device->mode_config.max_height = HEIGHT;
  state.device->mode_config.preferred_depth = 24;
  state.device->mode_config.prefer_shadow = 0;
  state.device->mode_config.funcs = &bs_funcs;
  // TODO: set state.device->mode_config.fb_base;

  res = drm_crtc_init(state.device, &state.crtc, &fake_disp_crtc_funcs);
  if (res) {
    goto fail_1;
  }
  drm_crtc_helper_add(&state.crtc, &fake_disp_crtc_helper_funcs);

  res = drm_encoder_init(state.device, &state.encoder, &fake_disp_enc_funcs,
                         DRM_MODE_ENCODER_VIRTUAL, NULL);
  if (res) {
    goto fail_2;
  }
  drm_encoder_helper_add(&state.encoder, &fake_disp_enc_helper_funcs);

  res = drm_connector_init(state.device, &state.connector,
                           &fake_disp_conn_funcs, DRM_MODE_CONNECTOR_VIRTUAL);
  if (res) {
    goto fail_3;
  }
  drm_connector_helper_add(&state.connector, &fake_disp_conn_helper_funcs);

  res = drm_mode_connector_attach_encoder(&state.connector, &state.encoder);
  if (res) {
    goto fail_4;
  }

  res = drm_dev_register(state.device, 0);
  if (res) {
    goto fail_4;
  }

  res = drm_connector_register(&state.connector);
  if (res) {
    goto fail_5;
  }

  return 0;

fail_5:
  drm_dev_unregister(state.device);
fail_4:
  drm_connector_cleanup(&state.connector);
fail_3:
  drm_encoder_cleanup(&state.encoder);
fail_2:
  drm_crtc_cleanup(&state.crtc);
fail_1:
  drm_mode_config_cleanup(state.device);
  drm_dev_put(state.device);
  return res;
}

static void __exit fake_disp_exit(void) {
  drm_connector_unregister(&state.connector);
  drm_dev_unregister(state.device);
  drm_connector_cleanup(&state.connector);
  drm_encoder_cleanup(&state.encoder);
  drm_crtc_cleanup(&state.crtc);
  drm_mode_config_cleanup(state.device);
  drm_dev_put(state.device);
}

module_init(fake_disp_init);
module_exit(fake_disp_exit);
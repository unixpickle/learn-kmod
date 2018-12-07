#include "fake_disp.h"

// Planes

static const u32 fake_disp_plane_format = DRM_FORMAT_RGB888;

static void fake_disp_plane_atomic_update(struct drm_plane* plane,
                                          struct drm_plane_state* state) {
  printk("fake_disp plane_atomic_update (pid=%d)\n", task_pid_nr(current));
}

static const struct drm_plane_helper_funcs fake_disp_plane_helper_funcs = {
    .atomic_update = fake_disp_plane_atomic_update,
};

static const struct drm_plane_funcs fake_disp_plane_funcs = {
    .update_plane = drm_atomic_helper_update_plane,
    .disable_plane = drm_atomic_helper_disable_plane,
    .destroy = drm_plane_cleanup,
    .reset = drm_atomic_helper_plane_reset,
    .atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

// CRTC

static enum drm_mode_status fake_disp_crtc_mode_valid(
    struct drm_crtc* crtc,
    const struct drm_display_mode* mode) {
  printk(KERN_INFO "fake_disp crtc_mode_valid %d %d (pid=%d)\n", mode->hdisplay,
         mode->vdisplay, task_pid_nr(current));
  return MODE_OK;
}

static void fake_disp_crtc_mode_set_nofb(struct drm_crtc* crtc) {
  printk(KERN_INFO "fake_disp crtc_mode_set_nofb (pid=%d)\n",
         task_pid_nr(current));
}

static void fake_disp_crtc_atomic_enable(struct drm_crtc* crtc,
                                         struct drm_crtc_state* old_state) {
  printk(KERN_INFO "fake_disp crtc_atomic_enable (pid=%d)\n",
         task_pid_nr(current));
}

static void fake_disp_crtc_atomic_disable(struct drm_crtc* crtc,
                                          struct drm_crtc_state* old_state) {
  printk(KERN_INFO "fake_disp crtc_atomic_disable (pid=%d)\n",
         task_pid_nr(current));
}

static void fake_disp_crtc_atomic_begin(struct drm_crtc* crtc,
                                        struct drm_crtc_state* state) {
  struct drm_pending_vblank_event* event;
  printk(KERN_INFO "fake_disp crtc_atomic_begin (pid=%d)\n",
         task_pid_nr(current));
  event = crtc->state->event;
  if (event) {
    crtc->state->event = NULL;
    spin_lock_irq(&crtc->dev->event_lock);
    drm_crtc_send_vblank_event(crtc, event);
    spin_unlock_irq(&crtc->dev->event_lock);
  }
}

static const struct drm_crtc_funcs fake_disp_crtc_funcs = {
    .destroy = drm_crtc_cleanup,
    .set_config = drm_atomic_helper_set_config,
    .page_flip = drm_atomic_helper_page_flip,
    .reset = drm_atomic_helper_crtc_reset,
    .atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

static const struct drm_crtc_helper_funcs fake_disp_crtc_helper_funcs = {
    .mode_valid = fake_disp_crtc_mode_valid,
    .mode_set = drm_helper_crtc_mode_set,
    .mode_set_base = drm_helper_crtc_mode_set_base,
    .mode_set_nofb = fake_disp_crtc_mode_set_nofb,
    .atomic_begin = fake_disp_crtc_atomic_begin,
    .atomic_enable = fake_disp_crtc_atomic_enable,
    .atomic_disable = fake_disp_crtc_atomic_disable,
};

// Connector

static int fake_disp_conn_get_modes(struct drm_connector* connector) {
  int res = drm_add_modes_noedid(connector, WIDTH, HEIGHT);
  drm_set_preferred_mode(connector, WIDTH, HEIGHT);
  printk(KERN_INFO "fake_disp conn_get_modes %d\n", res);
  return res;
}

static enum drm_mode_status fake_disp_conn_mode_valid(
    struct drm_connector* connector,
    struct drm_display_mode* mode) {
  printk(KERN_INFO "fake_disp conn_mode_valid %d %d (pid=%d)\n", mode->hdisplay,
         mode->vdisplay, task_pid_nr(current));
  return MODE_OK;
}

static struct drm_encoder* fake_disp_conn_best_encoder(
    struct drm_connector* connector) {
  return &fake_disp_get_state()->encoder;
}

static void fake_disp_drm_connector_destroy(struct drm_connector* connector) {
  drm_connector_unregister(connector);
  drm_connector_cleanup(connector);
}

static const struct drm_connector_helper_funcs fake_disp_conn_helper_funcs = {
    .get_modes = fake_disp_conn_get_modes,
    .mode_valid = fake_disp_conn_mode_valid,
    .best_encoder = fake_disp_conn_best_encoder,
};

static const struct drm_connector_funcs fake_disp_conn_funcs = {
    .dpms = drm_helper_connector_dpms,
    .reset = drm_atomic_helper_connector_reset,
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = fake_disp_drm_connector_destroy,
    .atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
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

// Mode config

static void fake_disp_output_poll_changed(struct drm_device* dev) {
  drm_fbdev_cma_hotplug_event(fake_disp_get_state()->fbdev);
}

static struct drm_mode_config_funcs fake_disp_mode_config_funcs = {
    .fb_create = fake_disp_user_framebuffer_create,
    .output_poll_changed = fake_disp_output_poll_changed,
    .atomic_check = drm_atomic_helper_check,
    .atomic_commit = drm_atomic_helper_commit,
};

// Driver

static int fake_disp_open(struct inode* inode, struct file* filp) {
  printk(KERN_INFO "fake_disp open() called (pid=%d).\n", task_pid_nr(current));
  return drm_open(inode, filp);
}

static long fake_disp_ioctl(struct file* filp,
                            unsigned int cmd,
                            unsigned long arg) {
  long res = drm_ioctl(filp, cmd, arg);
  printk(KERN_INFO "fake_disp ioctl(%d) -> %ld\n", cmd, res);
  return res;
}

static long fake_disp_compat_ioctl(struct file* filp,
                                   unsigned int cmd,
                                   unsigned long arg) {
  long res = drm_compat_ioctl(filp, cmd, arg);
  printk(KERN_INFO "fake_disp compat_ioctl(%d) -> %ld\n", cmd, res);
  return res;
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

static const struct vm_operations_struct fake_disp_gem_vm_ops = {
    .open = drm_gem_vm_open,
    .close = drm_gem_vm_close,
};

static struct drm_driver fake_disp_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
    .fops = &fake_disp_fops,
    .name = "fake_disp",
    .desc = "fake display interface",
    .date = "20181205",
    .major = 1,
    .minor = 0,
    .gem_vm_ops = &fake_disp_gem_vm_ops,
    .gem_free_object_unlocked = fake_disp_gem_free_object,
    .dumb_create = fake_disp_gem_dumb_create,
    .dumb_map_offset = fake_disp_gem_dumb_map_offset,
    .dumb_destroy = fake_disp_gem_dumb_destroy,
};

// Lifecycle

int fake_disp_setup_drm(void) {
  int res;
  struct fake_disp_state* state = fake_disp_get_state();

  state->device = drm_dev_alloc(&fake_disp_driver, NULL);
  if (IS_ERR(state->device)) {
    return PTR_ERR(state->device);
  }

  drm_mode_config_init(state->device);
  state->device->mode_config.min_width = 0;
  state->device->mode_config.min_height = 0;
  state->device->mode_config.max_width = WIDTH;
  state->device->mode_config.max_height = HEIGHT;
  state->device->mode_config.funcs = &fake_disp_mode_config_funcs;

  res = drm_universal_plane_init(
      state->device, &state->plane, 0xff, &fake_disp_plane_funcs,
      &fake_disp_plane_format, 1, NULL, DRM_PLANE_TYPE_PRIMARY, NULL);
  if (res) {
    goto fail_1;
  }
  drm_plane_helper_add(&state->plane, &fake_disp_plane_helper_funcs);

  res = drm_crtc_init_with_planes(state->device, &state->crtc, &state->plane,
                                  NULL, &fake_disp_crtc_funcs, NULL);
  if (res) {
    goto fail_2;
  }
  state->crtc.enabled = true;
  drm_crtc_helper_add(&state->crtc, &fake_disp_crtc_helper_funcs);

  res = drm_encoder_init(state->device, &state->encoder, &fake_disp_enc_funcs,
                         DRM_MODE_ENCODER_VIRTUAL, NULL);
  state->encoder.possible_crtcs = 1;
  if (res) {
    goto fail_3;
  }
  drm_encoder_helper_add(&state->encoder, &fake_disp_enc_helper_funcs);

  res = drm_connector_init(state->device, &state->connector,
                           &fake_disp_conn_funcs, DRM_MODE_CONNECTOR_VIRTUAL);
  if (res) {
    goto fail_4;
  }
  state->connector.status = connector_status_connected;
  drm_connector_helper_add(&state->connector, &fake_disp_conn_helper_funcs);

  res = drm_mode_connector_attach_encoder(&state->connector, &state->encoder);
  if (res) {
    goto fail_5;
  }

  res = drm_dev_register(state->device, 0);
  if (res) {
    goto fail_5;
  }

  res = drm_connector_register(&state->connector);
  if (res) {
    goto fail_6;
  }

  // Seems to prevent a NULL pointer dereference.
  drm_mode_config_reset(state->device);

  return 0;

fail_6:
  drm_dev_unregister(state->device);
fail_5:
  drm_connector_cleanup(&state->connector);
fail_4:
  drm_encoder_cleanup(&state->encoder);
fail_3:
  drm_crtc_cleanup(&state->crtc);
fail_2:
  drm_plane_cleanup(&state->plane);
fail_1:
  drm_mode_config_cleanup(state->device);
  drm_dev_put(state->device);

  return res;
}

void fake_disp_destroy_drm(void) {
  // TODO: deregister helpers?
  struct fake_disp_state* state = fake_disp_get_state();
  drm_connector_unregister(&state->connector);
  drm_dev_unregister(state->device);
  drm_connector_cleanup(&state->connector);
  drm_encoder_cleanup(&state->encoder);
  drm_crtc_cleanup(&state->crtc);
  drm_plane_cleanup(&state->plane);
  drm_mode_config_cleanup(state->device);
  drm_dev_put(state->device);
}

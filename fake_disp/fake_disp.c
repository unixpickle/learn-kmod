#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_encoder.h>
#include <drm/drm_gem.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

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

static void fake_disp_crtc_dpms(struct drm_crtc* crtc, int mode) {}

static int fake_disp_crtc_mode_set_base(struct drm_crtc* crtc,
                                        int x,
                                        int y,
                                        struct drm_framebuffer* old_fb) {
  return 0;
}

static int fake_disp_crtc_mode_set(struct drm_crtc* crtc,
                                   struct drm_display_mode* mode,
                                   struct drm_display_mode* adjusted_mode,
                                   int x,
                                   int y,
                                   struct drm_framebuffer* old_fb) {
  return 0;
}

static void fake_disp_crtc_nop(struct drm_crtc* crtc) {}

static int fake_disp_crtc_page_flip(struct drm_crtc* crtc,
                                    struct drm_framebuffer* fb,
                                    struct drm_pending_vblank_event* event,
                                    uint32_t page_flip_flags,
                                    struct drm_modeset_acquire_ctx* ctx) {
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
  return drm_add_modes_noedid(connector, WIDTH, HEIGHT);
}

static enum drm_mode_status fake_disp_conn_mode_valid(
    struct drm_connector* connector,
    struct drm_display_mode* mode) {
  if (mode->hdisplay != WIDTH || mode->vdisplay != HEIGHT) {
    return MODE_BAD;
  }
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
                                   struct drm_display_mode* adjusted_mode) {}

static void fake_disp_enc_dpms(struct drm_encoder* encoder, int state) {}

static void fake_disp_enc_nop(struct drm_encoder* encoder) {}

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

DEFINE_DRM_GEM_FOPS(fake_disp_fops);

static struct drm_driver fake_disp_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET,
    .fops = &fake_disp_fops,
    .name = "fake_disp",
    .desc = "fake display interface",
    .date = "20181205",
    .major = 1,
    .minor = 0,
};

// Module lifecycle

static int __init fake_disp_init(void) {
  int res;

  state.device = drm_dev_alloc(&fake_disp_driver, NULL);
  if (IS_ERR(state.device)) {
    return PTR_ERR(state.device);
  }

  drm_mode_config_init(state.device);
  state.device->mode_config.max_width = WIDTH;
  state.device->mode_config.max_height = HEIGHT;
  state.device->mode_config.preferred_depth = 24;
  state.device->mode_config.prefer_shadow = 0;
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
  res = drm_connector_register(&state.connector);
  if (res) {
    goto fail_4;
  }

  res = drm_mode_connector_attach_encoder(&state.connector, &state.encoder);
  if (res) {
    goto fail_5;
  }

  res = drm_dev_register(state.device, 0);
  if (res) {
    goto fail_5;
  }

  return 0;

fail_5:
  drm_connector_unregister(&state.connector);
fail_4:
  drm_connector_cleanup(&state.connector);
fail_3:
  drm_encoder_cleanup(&state.encoder);
fail_2:
  drm_crtc_cleanup(&state.crtc);
fail_1:
  drm_dev_put(state.device);
  return res;
}

static void __exit fake_disp_exit(void) {}

module_init(fake_disp_init);
module_exit(fake_disp_exit);
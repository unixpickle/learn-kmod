#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_encoder.h>
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

static const struct drm_crtc_helper_funcs fake_disp_helper_funcs = {
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

// Module lifecycle

static int __init fake_disp_init(void) {
  return 0;
}

static void __exit fake_disp_exit(void) {}

module_init(fake_disp_init);
module_exit(fake_disp_exit);
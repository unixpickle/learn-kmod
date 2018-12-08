#include "fake_disp.h"

// DRM framebuffers

static void fake_disp_fb_destroy(struct drm_framebuffer* fb) {
  drm_framebuffer_cleanup(fb);
  kfree(fb);
}

static int fake_disp_fb_create_handle(struct drm_framebuffer* fb,
                                      struct drm_file* file_priv,
                                      u32* handle) {
  struct fake_disp_state* state = fake_disp_get_state();
  if (fb != state->fbdev_fb) {
    printk(KERN_WARNING "fake_disp: unexpected FB in fb_create_handle");
    return -ENOMEM;
  }
  return drm_gem_handle_create(file_priv, &state->fbdev_gem->base, handle);
}

static struct drm_framebuffer_funcs fake_disp_fb_funcs = {
    .destroy = fake_disp_fb_destroy,
    .create_handle = fake_disp_fb_create_handle,
};

// Framebuffer device

static struct fb_ops fake_disp_fb_ops = {
    .owner = THIS_MODULE,
    DRM_FB_HELPER_DEFAULT_OPS,
    .fb_fillrect = drm_fb_helper_sys_fillrect,
    .fb_copyarea = drm_fb_helper_sys_copyarea,
    .fb_imageblit = drm_fb_helper_sys_imageblit,
    // TODO: mmap here?
};

static int fake_disp_fb_probe(struct drm_fb_helper* helper,
                              struct drm_fb_helper_surface_size* sizes) {
  struct drm_mode_fb_cmd2 mode_cmd = {0};
  struct fake_disp_state* state = fake_disp_get_state();
  unsigned int bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);
  unsigned long offset;
  size_t size;
  int res;

  printk(KERN_INFO "fake_disp: fb_probe (surface width=%d height=%d bpp=%d)\n",
         sizes->surface_width, sizes->surface_height, sizes->surface_bpp);

  if (state->fbdev_gem || state->fbdev_fb) {
    printk(KERN_WARNING "fake_disp: fb_probe already called!\n");
    return -ENOMEM;
  }

  mode_cmd.width = sizes->surface_width;
  mode_cmd.height = sizes->surface_height;
  mode_cmd.pitches[0] = sizes->surface_width * bytes_per_pixel;
  mode_cmd.pixel_format =
      drm_mode_legacy_fb_format(sizes->surface_bpp, sizes->surface_depth);

  size = mode_cmd.pitches[0] * mode_cmd.height;

  state->fbdev_gem = fake_disp_gem_create(state->device, size);
  if (IS_ERR(state->fbdev_gem)) {
    return PTR_ERR(state->fbdev_gem);
  }

  state->fbdev_helper.fbdev = framebuffer_alloc(0, state->device->dev);
  if (!state->fbdev_helper.fbdev) {
    printk(KERN_WARNING "fake_disp: failed to alloc fbi.\n");
    res = -ENOMEM;
    goto fail_1;
  }

  state->fbdev_fb = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
  if (!state->fbdev_fb) {
    printk(KERN_WARNING "fake_disp: failed to alloc framebuffer.\n");
    res = -ENOMEM;
    goto fail_2;
  }

  drm_helper_mode_fill_fb_struct(state->device, state->fbdev_fb, &mode_cmd);
  res =
      drm_framebuffer_init(state->device, state->fbdev_fb, &fake_disp_fb_funcs);
  if (res) {
    goto fail_3;
  }

  state->fbdev_helper.fb = state->fbdev_fb;
  state->fbdev_helper.fbdev->par = helper;
  state->fbdev_helper.fbdev->flags = FBINFO_FLAG_DEFAULT;
  state->fbdev_helper.fbdev->fbops = &fake_disp_fb_ops;

  drm_fb_helper_fill_fix(state->fbdev_helper.fbdev, state->fbdev_fb->pitches[0],
                         state->fbdev_fb->format->depth);
  drm_fb_helper_fill_var(state->fbdev_helper.fbdev, helper, sizes->fb_width,
                         sizes->fb_height);

  offset = state->fbdev_helper.fbdev->var.xoffset * bytes_per_pixel;
  offset +=
      state->fbdev_helper.fbdev->var.yoffset * state->fbdev_fb->pitches[0];
  state->fbdev_helper.fbdev->screen_base = state->fbdev_gem->memory + offset;
  state->fbdev_helper.fbdev->screen_size = size;

  return 0;

fail_3:
  kfree(state->fbdev_fb);
fail_2:
  framebuffer_release(state->fbdev_helper.fbdev);
fail_1:
  fake_disp_gem_free_object(&state->fbdev_gem->base);

  return res;
}

static struct drm_fb_helper_funcs fake_disp_fb_helper_funcs = {
    .fb_probe = fake_disp_fb_probe,
};

// Lifecycle

int fake_disp_setup_fbdev(void) {
  int res;
  struct fake_disp_state* state = fake_disp_get_state();

  state->fbdev_helper.funcs = &fake_disp_fb_helper_funcs;

  res = drm_fb_helper_init(state->device, &state->fbdev_helper, 1);
  if (res < 0) {
    printk(KERN_WARNING "fake_disp: drm_fb_helper_init() failed\n");
    return res;
  }

  res = drm_fb_helper_single_add_all_connectors(&state->fbdev_helper);
  if (res < 0) {
    printk(KERN_WARNING
           "fake_disp: drm_fb_helper_single_add_all_connectors() failed\n");
    goto fail;
  }

  drm_helper_disable_unused_functions(state->device);

  res = drm_fb_helper_initial_config(&state->fbdev_helper, 24);
  if (res < 0) {
    printk(KERN_WARNING "fake_disp: drm_fb_helper_initial_config() failed\n");
    goto fail;
  }

  return 0;

fail:
  drm_fb_helper_fini(&state->fbdev_helper);
  return res;
}

void fake_disp_destroy_fbdev(void) {
  struct fake_disp_state* state = fake_disp_get_state();
  struct fb_info* fbi = state->fbdev_helper.fbdev;
  unregister_framebuffer(fbi);
  framebuffer_release(fbi);
  if (state->fbdev_fb) {
    drm_framebuffer_unregister_private(state->fbdev_fb);
    drm_framebuffer_cleanup(state->fbdev_fb);
    kfree(state->fbdev_fb);
    fake_disp_gem_free_object(&state->fbdev_gem->base);
  }
  drm_fb_helper_fini(&state->fbdev_helper);
}

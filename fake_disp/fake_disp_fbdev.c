#include "fake_disp.h"

// Based on
// https://elixir.bootlin.com/linux/v4.15/source/drivers/gpu/drm/drm_fb_cma_helper.c

// DRM framebuffers

static struct drm_framebuffer_funcs fake_disp_fb_funcs = {
    .destroy = drm_gem_fb_destroy,
    .create_handle = drm_gem_fb_create_handle,
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
  struct fake_disp_gem_object* gem;
  struct fb_info* fbi;
  struct fake_disp_state* state = fake_disp_get_state();
  unsigned int bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);
  unsigned long offset;
  size_t size;
  int res;

  printk(KERN_INFO "fake_disp: fb_probe (surface width=%d height=%d bpp=%d)\n",
         sizes->surface_width, sizes->surface_height, sizes->surface_bpp);

  if (state->fbdev_fb) {
    printk(KERN_WARNING "fake_disp: fb_probe already called!\n");
    return -ENOMEM;
  }

  size = sizes->surface_width * sizes->surface_height * bytes_per_pixel;
  gem = fake_disp_gem_create(state->device, size);
  if (IS_ERR(gem)) {
    return PTR_ERR(gem);
  }

  fbi = framebuffer_alloc(0, state->device->dev);
  if (IS_ERR(fbi)) {
    printk(KERN_WARNING "fake_disp: failed to alloc fbi.\n");
    res = PTR_ERR(fbi);
    goto fail_1;
  }

  state->fbdev_fb = drm_gem_fbdev_fb_create(state->device, sizes, 0, &gem->base,
                                            &fake_disp_fb_funcs);
  if (IS_ERR(state->fbdev_fb)) {
    printk(KERN_WARNING "fake_disp: failed to alloc framebuffer.\n");
    res = PTR_ERR(state->fbdev_fb);
    goto fail_2;
  }

  state->fbdev_helper.fb = state->fbdev_fb;
  fbi->par = helper;
  fbi->flags = FBINFO_FLAG_DEFAULT;
  fbi->fbops = &fake_disp_fb_ops;

  drm_fb_helper_fill_fix(fbi, state->fbdev_fb->pitches[0],
                         state->fbdev_fb->format->depth);
  drm_fb_helper_fill_var(fbi, helper, sizes->fb_width, sizes->fb_height);

  offset = fbi->var.xoffset * bytes_per_pixel;
  offset += fbi->var.yoffset * state->fbdev_fb->pitches[0];
  fbi->screen_base = gem->memory + offset;
  fbi->screen_size = size;

  return 0;

fail_2:
  framebuffer_release(state->fbdev_helper.fbdev);
fail_1:
  drm_gem_object_put_unlocked(&gem->base);

  return res;
}

static struct drm_fb_helper_funcs fake_disp_fb_helper_funcs = {
    .fb_probe = fake_disp_fb_probe,
};

// Lifecycle

int fake_disp_setup_fbdev(void) {
  int res;
  struct fake_disp_state* state = fake_disp_get_state();

  drm_fb_helper_prepare(state->device, &state->fbdev_helper,
                        &fake_disp_fb_helper_funcs);

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
    drm_framebuffer_remove(state->fbdev_fb);
  }
  drm_fb_helper_fini(&state->fbdev_helper);
}

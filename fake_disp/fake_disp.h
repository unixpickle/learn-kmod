#ifndef __FAKE_DISP_H__
#define __FAKE_DISP_H__

#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane_helper.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid.h>

#define WIDTH 1920
#define HEIGHT 1080

struct fake_disp_gem_object;

struct fake_disp_state {
  struct drm_device* device;
  struct drm_plane plane;
  struct drm_crtc crtc;
  struct drm_encoder encoder;
  struct drm_connector connector;

  struct drm_fb_helper fbdev_helper;
  struct drm_framebuffer* fbdev_fb;
};

// fake_disp_main.c
struct fake_disp_state* fake_disp_get_state(void);

// fake_disp_mm.c
struct fake_disp_gem_object {
  struct drm_gem_object base;
  void* memory;
};

struct fake_disp_gem_object* fake_disp_gem_create(struct drm_device* dev,
                                                  size_t size);
int fake_disp_mmap(struct file* filp, struct vm_area_struct* vma);
int fake_disp_gem_dumb_create(struct drm_file* file,
                              struct drm_device* dev,
                              struct drm_mode_create_dumb* args);
int fake_disp_gem_dumb_map_offset(struct drm_file* file,
                                  struct drm_device* dev,
                                  uint32_t handle,
                                  uint64_t* offset);
int fake_disp_gem_dumb_destroy(struct drm_file* file_priv,
                               struct drm_device* dev,
                               uint32_t handle);
void fake_disp_gem_free_object(struct drm_gem_object* gem_obj);
struct drm_framebuffer* fake_disp_user_framebuffer_create(
    struct drm_device* dev,
    struct drm_file* filp,
    const struct drm_mode_fb_cmd2* mode_cmd);

// fake_disp_drm.c
int fake_disp_setup_drm(void);
void fake_disp_destroy_drm(void);

// fake_disp_fbdev.c
int fake_disp_setup_fbdev(void);
void fake_disp_destroy_fbdev(void);

#endif

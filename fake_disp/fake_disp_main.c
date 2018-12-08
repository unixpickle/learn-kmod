#include "fake_disp.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("A fake external monitor.");
MODULE_VERSION("0.01");

static struct fake_disp_state state;

struct fake_disp_state* fake_disp_get_state(void) {
  return &state;
}

extern unsigned int drm_debug;

static int __init fake_disp_init(void) {
  // Enable this to see a ton of log messages.
  // drm_debug = 0xffffffff;
  int res = fake_disp_setup_drm();
  if (res) {
    return res;
  }
  res = fake_disp_setup_fbdev();
  if (res) {
    fake_disp_destroy_drm();
    return res;
  }
  return 0;
}

static void __exit fake_disp_exit(void) {
  fake_disp_destroy_fbdev();
  fake_disp_destroy_drm();
}

module_init(fake_disp_init);
module_exit(fake_disp_exit);

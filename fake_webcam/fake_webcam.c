#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("A virtual webcam driver.");
MODULE_VERSION("0.01");

struct fw_info {
  struct mutex ioctl_lock;
  struct mutex queue_lock;

  struct video_device* dev;
  struct v4l2_device parent_dev;
  struct vb2_queue queue;
};

static struct fw_info fw_info;

// File operations

static struct v4l2_file_operations fw_fops = {
    .unlocked_ioctl = video_ioctl2,
};

// IOCTL operations

static struct v4l2_ioctl_ops fw_ioctl_ops = {

};

// Device operations

static void fw_video_device_release(struct video_device* dev) {
  video_device_release(dev);
  // TODO: see if this will be necessary.
  // media_entity_cleanup(&dev->entity);
}

// Video buffer operations

static struct vb2_ops fw_vb2_ops = {

};

// Module lifecycle

static int __init fw_init(void) {
  int res;

  memset(&fw_info, 0, sizeof(fw_info));
  mutex_init(&fw_info.ioctl_lock);
  mutex_init(&fw_info.queue_lock);

  strncpy(fw_info.parent_dev.name, "fake_webcam",
          sizeof(fw_info.parent_dev.name));
  res = v4l2_device_register(NULL, &fw_info.parent_dev);
  if (res) {
    goto fail;
  }

  fw_info.queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fw_info.queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_READ;
  fw_info.queue.dev = fw_info.parent_dev.dev;
  fw_info.queue.lock = &fw_info.queue_lock;
  fw_info.queue.ops = &fw_vb2_ops;
  fw_info.queue.mem_ops = &vb2_vmalloc_memops;
  fw_info.queue.buf_struct_size = sizeof(struct vb2_buffer);
  vb2_queue_init(&fw_info.queue);

  fw_info.dev = video_device_alloc();
  if (!fw_info.dev) {
    res = -ENOMEM;
    goto fail_unregister;
  }
  fw_info.dev->release = fw_video_device_release;
  fw_info.dev->v4l2_dev = &fw_info.parent_dev;
  strncpy(fw_info.dev->name, "Fake Webcam", sizeof(fw_info.dev->name));
  fw_info.dev->vfl_dir = VFL_DIR_RX;
  fw_info.dev->fops = &fw_fops;
  fw_info.dev->ioctl_ops = &fw_ioctl_ops;
  fw_info.dev->lock = &fw_info.ioctl_lock;
  fw_info.dev->queue = &fw_info.queue;
  return 0;

fail_unregister:
  v4l2_device_unregister(&fw_info.parent_dev);
  vb2_queue_release(&fw_info.queue);
fail:
  return res;
}

static void __exit fw_exit(void) {
  video_unregister_device(fw_info.dev);
  v4l2_device_unregister(&fw_info.parent_dev);
  vb2_queue_release(&fw_info.queue);
}

module_init(fw_init);
module_exit(fw_exit);
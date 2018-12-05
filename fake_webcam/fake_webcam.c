#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("A virtual webcam driver.");
MODULE_VERSION("0.01");

#define DRIVER_NAME "fake_webcam"

// Global state.

struct fw_info {
  struct mutex ioctl_lock;

  struct video_device* dev;
  struct v4l2_device parent_dev;
  struct vb2_queue queue;

  // Used for frame streaming.
  struct mutex frame_buffer_lock;
  char* frame_buffer;
  int is_first_frame;
  u64 next_time;

  // Used for the control interface.
  int ctrl_major;
  struct class* ctrl_class;
  struct device* ctrl_device;
};

static struct fw_info fw_info;

// Video format specification

static const char* fw_fmt_description = "RGB24";
static const u32 fw_fmt_pixelformat = V4L2_PIX_FMT_RGB24;
static const int fw_fmt_depth = 24;
static const int fw_fmt_width = 1280;
static const int fw_fmt_height = 720;
static const int fw_fmt_field = V4L2_FIELD_NONE;
static const int fw_fmt_colorspace = V4L2_COLORSPACE_SRGB;
static const int fw_fmt_std = V4L2_STD_525_60;
static const int fw_fmt_bytes = (1280 * 720 * 3);

// File operations

static struct v4l2_file_operations fw_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = video_ioctl2,
    .open = v4l2_fh_open,
    .release = vb2_fop_release,
    .read = vb2_fop_read,
    .poll = vb2_fop_poll,
    .mmap = vb2_fop_mmap,
};

// IOCTL operations

static int fw_vidioc_querycap(struct file* f,
                              void* priv,
                              struct v4l2_capability* cap) {
  strncpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
  strncpy(cap->card, DRIVER_NAME, sizeof(cap->card));
  strncpy(cap->bus_info, DRIVER_NAME, sizeof(cap->bus_info));
  cap->device_caps =
      V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
  cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
  return 0;
}

static int fw_vidioc_enum_framesizes(struct file* file,
                                     void* fh,
                                     struct v4l2_frmsizeenum* frm) {
  if (frm->index) {
    return -EINVAL;
  }
  frm->pixel_format = fw_fmt_pixelformat;
  frm->type = V4L2_FRMSIZE_TYPE_DISCRETE;
  frm->discrete.width = fw_fmt_width;
  frm->discrete.height = fw_fmt_height;
  return 0;
}

static int fw_vidioc_enum_frameintervals(struct file* f,
                                         void* priv,
                                         struct v4l2_frmivalenum* frm) {
  if (frm->index) {
    return -EINVAL;
  }
  frm->pixel_format = fw_fmt_pixelformat;
  frm->width = fw_fmt_width;
  frm->height = fw_fmt_height;
  frm->type = V4L2_FRMIVAL_TYPE_DISCRETE;
  frm->discrete.numerator = 1;
  frm->discrete.denominator = 60;
  return 0;
}

static int fw_vidioc_enum_fmt_vid_cap(struct file* f,
                                      void* priv,
                                      struct v4l2_fmtdesc* fmt) {
  if (fmt->index) {
    return -EINVAL;
  }
  strncpy(fmt->description, fw_fmt_description, sizeof(fmt->description));
  fmt->pixelformat = fw_fmt_pixelformat;
  return 0;
}

static int fw_vidioc_g_fmt_vid_cap(struct file* f,
                                   void* priv,
                                   struct v4l2_format* fmt) {
  fmt->fmt.pix.width = fw_fmt_width;
  fmt->fmt.pix.height = fw_fmt_height;
  fmt->fmt.pix.field = fw_fmt_field;
  fmt->fmt.pix.pixelformat = fw_fmt_pixelformat;
  fmt->fmt.pix.bytesperline = (fw_fmt_width * fw_fmt_depth) / 8;
  fmt->fmt.pix.sizeimage = fw_fmt_bytes;
  fmt->fmt.pix.colorspace = fw_fmt_colorspace;
  return 0;
}

static int fw_vidioc_enum_input(struct file* f,
                                void* priv,
                                struct v4l2_input* input) {
  if (input->index) {
    return -EINVAL;
  }
  input->type = V4L2_INPUT_TYPE_CAMERA;
  input->std = fw_fmt_std;
  strncpy(input->name, "Fake Webcam", sizeof(input->name));
  return 0;
}

static int fw_vidioc_g_input(struct file* f, void* priv, unsigned int* i) {
  *i = 0;
  return 0;
}

static int fw_vidioc_s_input(struct file* f, void* priv, unsigned int i) {
  if (i) {
    return -EINVAL;
  }
  return 0;
}

static int fw_vidioc_g_parm(struct file* file,
                            void* fh,
                            struct v4l2_streamparm* sp) {
  if (sp->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    return -EINVAL;

  sp->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
  sp->parm.capture.readbuffers = 4;
  sp->parm.capture.extendedmode = 0;
  sp->parm.capture.timeperframe.numerator = 1;
  sp->parm.capture.timeperframe.denominator = 60;
  return 0;
}

static struct v4l2_ioctl_ops fw_ioctl_ops = {
    // Capabilities and formats.
    .vidioc_querycap = fw_vidioc_querycap,
    .vidioc_enum_framesizes = fw_vidioc_enum_framesizes,
    .vidioc_enum_frameintervals = fw_vidioc_enum_frameintervals,
    .vidioc_enum_fmt_vid_cap = fw_vidioc_enum_fmt_vid_cap,
    .vidioc_g_fmt_vid_cap = fw_vidioc_g_fmt_vid_cap,
    .vidioc_s_fmt_vid_cap = fw_vidioc_g_fmt_vid_cap,
    .vidioc_try_fmt_vid_cap = fw_vidioc_g_fmt_vid_cap,
    .vidioc_enum_input = fw_vidioc_enum_input,
    .vidioc_g_input = fw_vidioc_g_input,
    .vidioc_s_input = fw_vidioc_s_input,
    .vidioc_g_parm = fw_vidioc_g_parm,
    .vidioc_s_parm = fw_vidioc_g_parm,

    // Buffer manipulation.
    .vidioc_reqbufs = vb2_ioctl_reqbufs,
    .vidioc_querybuf = vb2_ioctl_querybuf,
    .vidioc_qbuf = vb2_ioctl_qbuf,
    .vidioc_dqbuf = vb2_ioctl_dqbuf,
    .vidioc_streamon = vb2_ioctl_streamon,
    .vidioc_streamoff = vb2_ioctl_streamoff,
};

// Video buffer operations

static int fw_vb2_queue_setup(struct vb2_queue* queue,
                              unsigned int* nbuffers,
                              unsigned int* nplanes,
                              unsigned int sizes[],
                              struct device* alloc_devs[]) {
  *nbuffers = 4;
  *nplanes = 1;
  sizes[0] = fw_fmt_bytes;
  return 0;
}

static void fw_vb2_wait_prepare(struct vb2_queue* queue) {
  mutex_unlock(&fw_info.ioctl_lock);
}

static void fw_vb2_wait_finish(struct vb2_queue* queue) {
  mutex_lock(&fw_info.ioctl_lock);
}

static int fw_vb2_start_streaming(struct vb2_queue* queue, unsigned int count) {
  fw_info.is_first_frame = 1;
  return 0;
}

static void fw_vb2_stop_streaming(struct vb2_queue* queue) {
  vb2_wait_for_all_buffers(queue);
}

static u64 fw_get_nanotime(void) {
  struct timeval val;
  do_gettimeofday(&val);
  return (u64)val.tv_sec * 1000000000 + (u64)val.tv_usec * 1000;
}

static void fw_fill_buffer(struct vb2_buffer* buffer) {
  char* data = (char*)vb2_plane_vaddr(buffer, 0);
  mutex_lock(&fw_info.frame_buffer_lock);
  memcpy(data, fw_info.frame_buffer, fw_fmt_bytes);
  mutex_unlock(&fw_info.frame_buffer_lock);
  vb2_set_plane_payload(buffer, 0, fw_fmt_bytes);
}

static int fw_ship_buffer_thread(void* buf_void) {
  struct vb2_buffer* buffer = (struct vb2_buffer*)buf_void;

  u64 nanotime = fw_get_nanotime();
  if (nanotime < buffer->timestamp) {
    schedule_timeout_interruptible(
        nsecs_to_jiffies64(buffer->timestamp - nanotime));
  }

  fw_fill_buffer(buffer);
  vb2_buffer_done(buffer, VB2_BUF_STATE_DONE);

  do_exit(0);
  return 0;
}

static void fw_vb2_buf_queue(struct vb2_buffer* buffer) {
  u64 nanotime = fw_get_nanotime();
  if (fw_info.is_first_frame) {
    fw_info.is_first_frame = 0;
    fw_info.next_time = nanotime;
  } else if (fw_info.next_time < nanotime) {
    // The frames aren't being consumed fast enough.
    fw_info.next_time = nanotime;
  }
  buffer->timestamp = fw_info.next_time;
  kthread_run(fw_ship_buffer_thread, buffer, "bufqueue");
}

static struct vb2_ops fw_vb2_ops = {
    .queue_setup = fw_vb2_queue_setup,
    .wait_prepare = fw_vb2_wait_prepare,
    .wait_finish = fw_vb2_wait_finish,
    .start_streaming = fw_vb2_start_streaming,
    .stop_streaming = fw_vb2_stop_streaming,
    .buf_queue = fw_vb2_buf_queue,
};

// User-space control device.

typedef struct {
  unsigned char* write_buffer;
  size_t cur_written;
} fw_ctrl_t;

static int fw_ctrl_open(struct inode* inode, struct file* f) {
  fw_ctrl_t* ctrl = vmalloc(sizeof(fw_ctrl_t));
  if (!ctrl) {
    return -ENOMEM;
  }
  ctrl->cur_written = 0;
  ctrl->write_buffer = vmalloc(fw_fmt_bytes);
  if (!ctrl->write_buffer) {
    vfree(ctrl);
    return -ENOMEM;
  }
  f->private_data = ctrl;
  return 0;
}

static int fw_ctrl_release(struct inode* inode, struct file* f) {
  fw_ctrl_t* ctrl = (fw_ctrl_t*)f->private_data;
  vfree(ctrl->write_buffer);
  vfree(ctrl);
  return 0;
}

static ssize_t fw_ctrl_read(struct file* f,
                            char __user* data,
                            size_t size,
                            loff_t* off) {
  return 0;
}

static ssize_t fw_ctrl_write(struct file* f,
                             const char __user* data,
                             size_t size,
                             loff_t* off) {
  size_t remaining = size;
  fw_ctrl_t* ctrl = (fw_ctrl_t*)f->private_data;
  char* local_data = (char*)vmalloc(size);
  char* cur_data = local_data;
  if (!local_data) {
    return -ENOMEM;
  }
  copy_from_user(local_data, data, size);
  while (remaining) {
    size_t needed = fw_fmt_bytes - ctrl->cur_written;
    if (remaining < needed) {
      memcpy(&ctrl->write_buffer[ctrl->cur_written], cur_data, remaining);
      ctrl->cur_written += remaining;
      remaining = 0;
    } else {
      memcpy(&ctrl->write_buffer[ctrl->cur_written], cur_data, needed);
      mutex_lock(&fw_info.frame_buffer_lock);
      memcpy(fw_info.frame_buffer, ctrl->write_buffer, fw_fmt_bytes);
      mutex_unlock(&fw_info.frame_buffer_lock);
      ctrl->cur_written = 0;
      remaining -= needed;
      cur_data += needed;
    }
  }
  vfree(local_data);
  return size;
}

static struct file_operations fw_ctrl_fops = {
    .open = fw_ctrl_open,
    .release = fw_ctrl_release,
    .read = fw_ctrl_read,
    .write = fw_ctrl_write,
};

static int fw_ctrl_setup(void) {
  int res;

  fw_info.ctrl_major = register_chrdev(0, DRIVER_NAME, &fw_ctrl_fops);
  if (fw_info.ctrl_major < 0) {
    return fw_info.ctrl_major;
  }

  fw_info.ctrl_class = class_create(THIS_MODULE, DRIVER_NAME);
  if (IS_ERR(fw_info.ctrl_class)) {
    res = PTR_ERR(fw_info.ctrl_class);
    goto fail_unregister;
  }

  fw_info.ctrl_device =
      device_create(fw_info.ctrl_class, NULL, MKDEV(fw_info.ctrl_major, 0),
                    NULL, DRIVER_NAME);
  if (IS_ERR(fw_info.ctrl_device)) {
    res = PTR_ERR(fw_info.ctrl_device);
    goto fail_unclass;
  }

  return 0;

fail_unclass:
  class_destroy(fw_info.ctrl_class);
fail_unregister:
  unregister_chrdev(fw_info.ctrl_major, DRIVER_NAME);
  return res;
}

static void fw_ctrl_destroy(void) {
  device_destroy(fw_info.ctrl_class, MKDEV(fw_info.ctrl_major, 0));
  class_destroy(fw_info.ctrl_class);
  unregister_chrdev(fw_info.ctrl_major, DRIVER_NAME);
}

// Module lifecycle

static int __init fw_init(void) {
  int res;

  memset(&fw_info, 0, sizeof(fw_info));
  mutex_init(&fw_info.ioctl_lock);
  mutex_init(&fw_info.frame_buffer_lock);

  fw_info.frame_buffer = (char*)vmalloc(fw_fmt_bytes);
  if (!fw_info.frame_buffer) {
    return -ENOMEM;
  }

  res = fw_ctrl_setup();
  if (res) {
    goto fail;
  }

  strncpy(fw_info.parent_dev.name, DRIVER_NAME,
          sizeof(fw_info.parent_dev.name));
  res = v4l2_device_register(NULL, &fw_info.parent_dev);
  if (res) {
    goto fail_destroy_ctrl;
  }

  fw_info.queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fw_info.queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_READ;
  fw_info.queue.dev = fw_info.parent_dev.dev;
  fw_info.queue.ops = &fw_vb2_ops;
  fw_info.queue.mem_ops = &vb2_vmalloc_memops;
  fw_info.queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
  vb2_queue_init(&fw_info.queue);

  fw_info.dev = video_device_alloc();
  if (!fw_info.dev) {
    res = -ENOMEM;
    goto fail_unregister;
  }
  fw_info.dev->release = video_device_release;
  fw_info.dev->v4l2_dev = &fw_info.parent_dev;
  strncpy(fw_info.dev->name, "Fake Webcam", sizeof(fw_info.dev->name));
  fw_info.dev->vfl_dir = VFL_DIR_RX;
  fw_info.dev->fops = &fw_fops;
  fw_info.dev->ioctl_ops = &fw_ioctl_ops;
  fw_info.dev->lock = &fw_info.ioctl_lock;
  fw_info.dev->queue = &fw_info.queue;
  fw_info.dev->tvnorms = fw_fmt_std;

  res = video_register_device_no_warn(fw_info.dev, VFL_TYPE_GRABBER, -1);
  if (res < 0) {
    goto fail_device_free;
  }

  return 0;

fail_device_free:
  video_device_release(fw_info.dev);
fail_unregister:
  v4l2_device_unregister(&fw_info.parent_dev);
  vb2_queue_release(&fw_info.queue);
fail_destroy_ctrl:
  fw_ctrl_destroy();
fail:
  vfree(fw_info.frame_buffer);
  return res;
}

static void __exit fw_exit(void) {
  video_unregister_device(fw_info.dev);
  v4l2_device_unregister(&fw_info.parent_dev);
  vfree(fw_info.frame_buffer);
  fw_ctrl_destroy();
}

module_init(fw_init);
module_exit(fw_exit);
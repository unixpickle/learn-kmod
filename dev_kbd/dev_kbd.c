#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("A user-controllable keyboard device.");
MODULE_VERSION("0.01");

#define DRIVER_NAME "dev_kbd"

struct dev_kbd_state {
  // For the fake keyboard.
  struct input_dev* input_dev;

  // For the character device.
  int ctrl_major;
  struct class* ctrl_class;
  struct device* ctrl_device;
};

static struct dev_kbd_state state;

// Control interface

#define CTRL_BUFFER 128

struct dev_kbd_ctrl {
  unsigned char* write_buffer;
  int cur_written;
};

static int dev_kbd_ctrl_open(struct inode* inode, struct file* f) {
  struct dev_kbd_ctrl* ctrl = vmalloc(sizeof(struct dev_kbd_ctrl));
  if (!ctrl) {
    return -ENOMEM;
  }
  ctrl->cur_written = 0;
  ctrl->write_buffer = vmalloc(CTRL_BUFFER);
  if (!ctrl->write_buffer) {
    vfree(ctrl);
    return -ENOMEM;
  }
  f->private_data = ctrl;
  return 0;
}

static int dev_kbd_ctrl_release(struct inode* inode, struct file* f) {
  struct dev_kbd_ctrl* ctrl = (struct dev_kbd_ctrl*)f->private_data;
  vfree(ctrl->write_buffer);
  vfree(ctrl);
  return 0;
}

static ssize_t dev_kbd_ctrl_read(struct file* f,
                                 char __user* data,
                                 size_t size,
                                 loff_t* off) {
  return 0;
}

static void dev_kbd_ctrl_handle(int code, int value) {
  input_report_key(state.input_dev, code, value);
}

static void dev_kbd_ctrl_handle_line(struct dev_kbd_ctrl* ctrl) {
  int comma_index = 0;
  int num_commas = 0;
  long code;
  long value;
  int i;
  for (i = 0; i < ctrl->cur_written; ++i) {
    if (ctrl->write_buffer[i] == ',') {
      comma_index = i;
      ++num_commas;
    }
  }
  if (num_commas != 1) {
    return;
  }
  ctrl->write_buffer[comma_index] = 0;
  if (kstrtol(ctrl->write_buffer, 10, &code)) {
    return;
  }
  if (kstrtol(&ctrl->write_buffer[comma_index + 1], 10, &value)) {
    return;
  }
  dev_kbd_ctrl_handle((int)code, (int)value);
}

static ssize_t dev_kbd_ctrl_write(struct file* f,
                                  const char __user* data,
                                  size_t size,
                                  loff_t* off) {
  size_t i;
  struct dev_kbd_ctrl* ctrl = (struct dev_kbd_ctrl*)f->private_data;
  char* local_data = (char*)vmalloc(size);
  if (!local_data) {
    return -ENOMEM;
  }
  copy_from_user(local_data, data, size);
  for (i = 0; i < size; ++i) {
    if (ctrl->cur_written + 1 == CTRL_BUFFER) {
      // Deal with overflow by forgetting everything.
      ctrl->cur_written = 0;
    }
    if (local_data[i] == '\n') {
      ctrl->write_buffer[ctrl->cur_written] = 0;
      dev_kbd_ctrl_handle_line(ctrl);
      ctrl->cur_written = 0;
    } else {
      ctrl->write_buffer[ctrl->cur_written++] = local_data[i];
    }
  }
  vfree(local_data);
  return size;
}

static struct file_operations dev_kbd_ctrl_fops = {
    .open = dev_kbd_ctrl_open,
    .release = dev_kbd_ctrl_release,
    .read = dev_kbd_ctrl_read,
    .write = dev_kbd_ctrl_write,
};

static int dev_kbd_ctrl_init(void) {
  int res;

  state.ctrl_major = register_chrdev(0, DRIVER_NAME, &dev_kbd_ctrl_fops);
  if (state.ctrl_major < 0) {
    return state.ctrl_major;
  }

  state.ctrl_class = class_create(THIS_MODULE, DRIVER_NAME);
  if (IS_ERR(state.ctrl_class)) {
    res = PTR_ERR(state.ctrl_class);
    goto fail_unregister;
  }

  state.ctrl_device = device_create(
      state.ctrl_class, NULL, MKDEV(state.ctrl_major, 0), NULL, DRIVER_NAME);
  if (IS_ERR(state.ctrl_device)) {
    res = PTR_ERR(state.ctrl_device);
    goto fail_unclass;
  }

  return 0;

fail_unclass:
  class_destroy(state.ctrl_class);
fail_unregister:
  unregister_chrdev(state.ctrl_major, DRIVER_NAME);
  return res;
}

static void dev_kbd_ctrl_destroy(void) {
  device_destroy(state.ctrl_class, MKDEV(state.ctrl_major, 0));
  class_destroy(state.ctrl_class);
  unregister_chrdev(state.ctrl_major, DRIVER_NAME);
}

// Module lifecycle

static int __init dev_kbd_init(void) {
  int i;
  int res = dev_kbd_ctrl_init();
  if (res) {
    return res;
  }

  state.input_dev = input_allocate_device();
  if (!state.input_dev) {
    goto fail_alloc;
  }

  state.input_dev->name = DRIVER_NAME;
  state.input_dev->phys = DRIVER_NAME;
  state.input_dev->uniq = DRIVER_NAME;
  set_bit(EV_KEY, state.input_dev->evbit);
  for (i = 2; i < 80; i++) {
    set_bit(i, state.input_dev->keybit);
  }

  res = input_register_device(state.input_dev);
  if (res) {
    goto fail_reg;
  }

  return 0;

fail_reg:
  input_free_device(state.input_dev);
fail_alloc:
  dev_kbd_ctrl_destroy();

  return res;
}

static void __exit dev_kbd_exit(void) {
  dev_kbd_ctrl_destroy();
  input_unregister_device(state.input_dev);
}

module_init(dev_kbd_init);
module_exit(dev_kbd_exit);
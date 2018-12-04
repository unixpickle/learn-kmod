# Overview

This will be a fake webcam driver. Ideally, it will allow a user-space process to decide the images "captured" by the webcam.

# Installing

First, compile everything with `make`.

Next, load the module dependencies:

```shell
sudo modprobe videodev
sudo modprobe videobuf2-core
sudo modprobe videobuf2-vmalloc
sudo modprobe videobuf2-v4l2
```

Next, add the fake_webcam module to your kernel:

```shell
sudo insmod build/fake_webcam.ko
```

# Sources

 * https://elixir.bootlin.com/linux/v3.4/source/drivers/media/video/vivi.c
 * https://linuxtv.org/downloads/v4l-dvb-apis/kapi/v4l2-dev.html
 * http://derekmolloy.ie/writing-a-linux-kernel-module-part-2-a-character-device/#Character_Device_Drivers
 * Linux headers

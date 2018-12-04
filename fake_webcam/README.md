# Overview

This is driver that pretends to be a webcam, but really just shows a buffer that is controlled by a user-space program.

User-space programs can update the picture by writing an RGB image to `/dev/fake_webcam`. For an example, see [bouncy_ball.c](bouncy_ball.c).

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

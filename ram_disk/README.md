# Overview

This is a simple RAM disk implementation. It creates a block device at `/dev/ramdisk` that stores its contents in kernel memory. It serves as a minimal example for creating a block device on Linux.

# Sources

 * https://blog.sourcerer.io/writing-a-simple-linux-kernel-module-d9dc3762c234
 * https://linux-kernel-labs.github.io/master/labs/block_device_drivers.html
 * https://elixir.bootlin.com/linux/v4.3/source/include/linux/fs.h#L450
 * https://elixir.bootlin.com/linux/latest/source/include/linux/bvec.h#L30
 * https://elixir.bootlin.com/linux/latest/source/include/linux/blk_types.h#L144
 * https://elixir.bootlin.com/linux/latest/source/include/linux/blkdev.h#L150
 * https://static.lwn.net/images/pdf/LDD3/ch16.pdf

# virt-fs

This is an extremely simple virtual filesystem that exposes the contents of a tar file.

# Usage

```
$ make
$ sudo insmod build/virt_fs.ko
$ mkdir mnt
$ mount -t virt_fs none mnt
```

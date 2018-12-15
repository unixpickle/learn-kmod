# virt-fs

This is an extremely simple virtual filesystem that exposes a fake file called "file.txt".

# Usage

```
$ make
$ sudo insmod build/virt_fs.ko
$ mkdir mnt
$ mount -t virt_fs none mnt
```

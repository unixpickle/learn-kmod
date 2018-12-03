#include <asm-generic/errno-base.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Nichol");
MODULE_DESCRIPTION("A simple in-memory block device.");
MODULE_VERSION("0.01");

struct ram_disk_info {
  spinlock_t lock;
  unsigned char* data;
  size_t size;
  struct request_queue* queue;
  struct gendisk* disk;
  int major;
};

static void ram_disk_request(struct request_queue* queue);
static void ram_disk_page_op(struct request* req,
                             struct bio_vec* bvec,
                             struct req_iterator* ri);
static int ram_disk_open(struct block_device* dev, fmode_t mode);
static void ram_disk_release(struct gendisk* disk, fmode_t mode);
static int ram_disk_ioctl(struct block_device* dev,
                          fmode_t mode,
                          unsigned int x,
                          unsigned long y);
static void ram_disk_free(void);

static struct ram_disk_info info;
static struct block_device_operations ram_disk_ops = {
    .open = ram_disk_open,
    .release = ram_disk_release,
    .ioctl = ram_disk_ioctl,
    .owner = THIS_MODULE,
};

static void ram_disk_request(struct request_queue* queue) {
  struct request* req;
  while ((req = blk_fetch_request(queue))) {
    if (!blk_rq_is_passthrough(req)) {
      struct bio_vec bvec;
      struct req_iterator iter;
      rq_for_each_segment(bvec, req, iter) ram_disk_page_op(req, &bvec, &iter);
    }
    __blk_end_request_all(req, 0);
  }
}

static void ram_disk_page_op(struct request* req,
                             struct bio_vec* bvec,
                             struct req_iterator* ri) {
  size_t start = (size_t)ri->iter.bi_sector * 512;
  if (start + (size_t)bvec->bv_len > info.size) {
    printk(KERN_INFO, "Out of bounds RAM disk access!");
    return;
  }
  char* page_addr = kmap_atomic(bvec->bv_page);
  if (rq_data_dir(req) == WRITE) {
    memcpy(&info.data[start], &page_addr[bvec->bv_offset], bvec->bv_len);
  } else {
    memcpy(&page_addr[bvec->bv_offset], &info.data[start], bvec->bv_len);
  }
  kunmap_atomic(page_addr);
}

static int ram_disk_open(struct block_device* dev, fmode_t mode) {
  return 0;
}

static void ram_disk_release(struct gendisk* disk, fmode_t mode) {}

static int ram_disk_ioctl(struct block_device* dev,
                          fmode_t mode,
                          unsigned int x,
                          unsigned long y) {
  return -ENOTTY;
}

static void ram_disk_free(void) {
  if (info.data) {
    vfree(info.data);
  }
  if (info.queue) {
    blk_cleanup_queue(info.queue);
  }
  if (info.disk) {
    del_gendisk(info.disk);
  }
}

static int __init ram_disk_init(void) {
  printk(KERN_INFO "Loading RAMDisk module\n");
  spin_lock_init(&info.lock);
  info.size = 1 << 20;
  info.data = vmalloc(info.size);
  if (!info.data) {
    goto fail;
  }
  info.queue = blk_init_queue(ram_disk_request, &info.lock);
  if (!info.queue) {
    goto fail;
  }
  info.disk = alloc_disk(1);
  if (!info.disk) {
    goto fail;
  }
  info.major = register_blkdev(0, "ram_disk");
  if (info.major <= 0) {
    goto fail;
  }
  info.disk->major = info.major;
  info.disk->first_minor = 0;
  info.disk->fops = &ram_disk_ops;
  info.disk->queue = info.queue;
  memcpy(info.disk->disk_name, "ramdisk", 8);
  set_capacity(info.disk, info.size / 512);
  add_disk(info.disk);

  return 0;

fail:
  ram_disk_free();
  return -ENOMEM;
}

static void __exit ram_disk_exit(void) {
  printk(KERN_INFO "Unloading RAMDisk module\n");
  ram_disk_free();
  unregister_blkdev(info.major, "ram_disk");
}

module_init(ram_disk_init);
module_exit(ram_disk_exit);
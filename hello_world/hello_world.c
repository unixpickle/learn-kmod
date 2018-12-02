// Taken from
// https://blog.sourcerer.io/writing-a-simple-linux-kernel-module-d9dc3762c234

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert W.Oliver II");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("0.01");

static int __init hello_world_init(void) {
  printk(KERN_INFO "Hello, World !\n");
  return 0;
}

static void __exit hello_world_exit(void) {
  printk(KERN_INFO "Goodbye, World !\n");
}

module_init(hello_world_init);
module_exit(hello_world_exit);
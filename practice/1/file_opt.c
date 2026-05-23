// 1. "Hello" Kernel program in Linux Kernel


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init my_init(void)
{
	printk(KERN_INFO"HELLO KERNEL\n");
	return 0;
	
}

static void __exit my_exit(void)
{
	printk(KERN_INFO"GOODBYE \n");

}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("FIRST KERNEL CODE");
MODULE_VERSION("1.0");

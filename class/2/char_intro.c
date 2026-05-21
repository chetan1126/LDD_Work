#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>

#define	DEVICE_NAME	"char_demo"
#define	DEVICE_COUNT	1


static dev_t dev_number;
static struct cdev char_dev;

static int my_open(struct inode *inode, struct file *file)
{
	pr_info("Character device demo\n");
        pr_info("major= %d, minor= %d\n",MAJOR(inode->i_rdev),MINOR(inode->i_rdev));

	return 0;


}

static int my_release(struct inode *inode, struct file *file)
{
	pr_info("release file\n");
	return 0;


}

static const struct file_operations fops =
{	
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release

};

static int __init my_init(void)
{
	int ret;
	pr_info("character Module Loading\n");
	
	ret = alloc_chrdev_region(&dev_number, 0 , DEVICE_COUNT, DEVICE_NAME);
	
	if(ret <0)
	{
		pr_err("char_demo: failed to allocate device number\n");
		return ret;
	}

	pr_info("char_demo: allocated major= %d, minor= %d\n",MAJOR(dev_number),MINOR(dev_number));
	
	
	cdev_init(&char_dev, &fops);
	char_dev.owner = THIS_MODULE;
	ret = cdev_add(&char_dev, dev_number, DEVICE_COUNT);
	if(ret <0)
	{
		pr_err("chat_demo : failed to add cdev\n");
		unregister_chrdev_region(dev_number, DEVICE_COUNT);
		return ret;
		
	}
	pr_info("char_demo: cdev added successfully\n");
	return 0;
}


static void __exit my_exit(void)
{
	pr_info("char_demo: module unloading\n");

	cdev_del(&char_dev);
	unregister_chrdev_region(dev_number, DEVICE_COUNT);
	pr_info("char_demo: module unloaded\n");


}

module_init(my_init);
module_exit(my_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("INTRO CHARACTER CODE");
MODULE_VERSION("1.0");

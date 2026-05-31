#include <linux/module.h>        
#include <linux/init.h>          
#include <linux/fs.h>            
#include <linux/cdev.h>          
#include <linux/device.h>        
#include <linux/uaccess.h>       
#include <linux/timer.h>         
#include <linux/jiffies.h>      
#include <linux/atomic.h>        

#define DRIVER_NAME "low_timer"
#define DEVICE_NAME "low_timer_dev"
#define CLASS_NAME  "low_timer_class"
#define TIMER_DELAY_MS 1000      

static dev_t dev_num;
static struct cdev timer_cdev;
static struct class *timer_class;
static struct device *timer_device;
static struct timer_list my_timer;
static atomic_t timer_count = ATOMIC_INIT(0);
static void timer_callback(struct timer_list *t)
{
	int count;
	count = atomic_inc_return(&timer_count);

	pr_info("%s: callback executed, timer_count=%d\n", DRIVER_NAME, count);
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIMER_DELAY_MS));
}

static int my_open(struct inode *inode, struct file *file)
{
	return 0;
}
static int my_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t my_read(struct file *file,
		       char __user *buf,
		       size_t len,
		       loff_t *off)
{
	char out[64];
	int n;
	n = scnprintf(out, sizeof(out), "timer_count = %d\n", atomic_read(&timer_count));
	return simple_read_from_buffer(buf, len, off, out, n);
}

static const struct file_operations fops =
{
	.owner   = THIS_MODULE,
	.open    = my_open,
	.release = my_release,
	.read    = my_read,
};

static int __init low_timer_init(void)
{
	int ret;
	ret = alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME);
	if (ret < 0)
		return ret;
	cdev_init(&timer_cdev, &fops);
	timer_cdev.owner = THIS_MODULE;
	ret = cdev_add(&timer_cdev, dev_num, 1);
	if (ret < 0)
       	{
		unregister_chrdev_region(dev_num, 1);
		return ret;
	}
	timer_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(timer_class)) {
		ret = PTR_ERR(timer_class);
		cdev_del(&timer_cdev);
		unregister_chrdev_region(dev_num, 1);
		return ret;
	} 
	timer_setup(&my_timer, timer_callback, 0);
	timer_device = device_create(timer_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(timer_device)) {
		ret = PTR_ERR(timer_device);
		class_destroy(timer_class);
		cdev_del(&timer_cdev);
		unregister_chrdev_region(dev_num, 1);
		return ret;
	}
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIMER_DELAY_MS));

	pr_info("%s: loaded successfully. Node /dev/%s created. HZ=%d\n", 
		DRIVER_NAME, DEVICE_NAME, HZ);
	return 0;
}

static void __exit low_timer_exit(void)
{
	del_timer_sync(&my_timer);
	device_destroy(timer_class, dev_num);
	class_destroy(timer_class);
	cdev_del(&timer_cdev);
	unregister_chrdev_region(dev_num, 1);

	pr_info("%s: unloaded successfully\n", DRIVER_NAME);
}

module_init(low_timer_init);
module_exit(low_timer_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("Periodic Counter Using Low-Resolution Kernel Timer");



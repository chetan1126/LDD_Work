#include<linux/module.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/uaccess.h>
#include<linux/timer.h>		// struct timer_list and kernel timer APIs
#include<linux/jiffies.h>	// provide jiffies, HZ, msec_to_jiffies, usec_to_jiffies
#include<linux/atomic.h>	//Provide atomic operation 
#include<linux/string.h>	
#include<linux/minmax.h>	// provide min max macro
#include<linux/compiler.h>		// Provide READ_ONCE and WRITE_ONCE opeation
				// read directly read to kernel and write directly write to kernel
#include<linux/kernel.h>

#define		DRIVER_NAME 	"timer_lab"
#define		DEVICE_NAME	"timer_lab"
#define		CLASS_NAME	"timer_lab_class"
#define		CMD_BUF		64		//maximun command size accespted from user 

#define		TIMER_STOPPED	0
#define 	TIMER_PERIODIC	1
#define		TIMER_ONESHOT	2

static dev_t dev_num;
static struct cdev timer_cdev;
static struct class *timer_class;
static struct device *timer_device;
static dtruct timer_list lab_timer;		// low resolution kernel timer object

static atomic_t tick_count = ATOMIC_INIT(0);	// count how many times the timer callback executed

static atomic_t timer_mode = ATOMIC_INIT(TIMER_STOPPED);	//store current timer mode: stopped, periodic or one-shot

static unsigned int period_ms = 1000;	//timer period in millisecond

static unsigned long last_tick_jiffies;	// stores jiffies value when timer callback is executed

static unsigned long next_expiry_jiffies;	//stores next programmed expiry time in jiffies

static const char *mode_to_string(int mode)
{
	//convert timer mode integer into redable string
	
	switch(mode)
	{
		case TIMER_STOPPED:
				return "stopped";
		case TIMER_PERIODIC:
				return "periodic";
		case TIMER_ONESHOT:
				return "one_shot";
		default :
				return "unknown";


	}
}

static unsigned long make_entry_from_ms(unsigned int ms)
{
	// convert milliseconds into an absolute future jiffies expiry value
	unsigned long delay_jiffies;

	delay_jiffies = msec_to_jiffies(ms);

	if(delay_jiffies == 0)		//safety check for very small value
		delay_jiffies =1;	// This forces atleast 1 jiffies delay

	return jiffies + delay_jiffies;
}

static int __init my_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&dev_num,0,1,DRIVER_NAME);

	if(ret < 0)return ret;

	cdev_init(&timer_cdev, &fops);

	time_cdev.owner = THIS_MODULE;

	ret = cdev_add(&timer_cdev, dev_num, 1);

	if(ret < 0)
	{
		unregiser_chrdev_region(dev_num,1);
		return ret;
	}
	
	timer_class = class_create(CLASS_NAME);

	if(IS_ERR(timer_class))
	{
		ret = PTR_ERR(time_class);
		cdev_del(&timer_cdev);
		unregister_chrdev_region(dev_num,1);
		return ret;
	}

	timer_setup(&lab_timer, timer_callback,0);

	timer_device = device_create(timer_class,NULL,dev_num,NULL,DEVICE_NAME);

	if(IS_ERR(timer_device))
	{
		ret = PTR_ERR(timer_device);
		class_destroy(timer_class);
		cdev_del(&timer_cdev);
		unregister_chrdev_region(dev_num,1);
		return ret;
	}

	pr_info("%s: loaded major %d, minor %d HS %d\n", DRIVER_NAME,MAJOR(dev_num),MINOR(dev_num),HZ);

	pr_info("%S: command: start <ms> oneshot <ms> peroid <ms> stop reset\n", DRIVER_NAME);
	return 0;
}

static int my_open(struct inode *inode, struct file*file)
{
	pr_info("%s: open\n", DRIVER_NAME);
	return 0;
}

static int my_release(struct inode*inode, struct file* file)
{
	pr_info("%s: release\n",DRIVER_NAME);
	return 0;
}

static ssize_t my_read(struct file* file , char __user*buf, size_t len, loff_t *off)
{
	char out[256];
	int n;
	int mode;
	int pending;		//stores whether timer is pending
	unsigned long now;	// stores current jiffies
	unsigned long next;	//store last expiry jiffies
	unsigned long last;	//store last callback jiffies
	unsigned long remaining_ms =0;
	mode = atomic_read(&timer_mode);	//Reading timer mode
	pending = timer_pending(&lab_timer);
	now = jiffies;
	next = READ_ONCE(next_expiry_jiffies);
	last = READ_ONCE(last_expiry_jiffies);


	//check if timer is pending and expiry is in near future
	if(pending && timer_after(next, now))
	{
		remaining_ms = jiffies_to_msec(next-now);
	}

	n = scnprintf(out, sizeof(out),"mode=%s\n" 
			"period %s\n"
			"tick_count= %d\n"
			"timer_pending= %d\n"
			"HZ =%d \n"
			"now_jiffies = %lu\n"
			"remaining_ms = %lu\n"
			"last_tick_jiffies = %lu\n",
			mode_to_string(mode),
			READ_ONCE(period_ms),
			atomic_read(&tick_count),
			pending,
			HZ,
			now,
			next,
			remaining_ms,
			last);

	return simple_read_from_buffer(buf, len, off,out,n);
	



static const struct file_operations fops ={
	.owner = THIS_MODULE,
	.open = mu_open,
	.release = my_release,
	.read = my_read,
	.write = my_write,
}

static void __exit my_exit(void)
{
	atomic_set(&timer_mode,TIMER_STOPPED);
	timer_shutdown_sync(&lab_timer);
	device_destroy(timer_class,dev_num);
	class_destroy(timer_class);
	cdev_del(&timer_cdev);
	unregister_chrdev_region(dev_num,1);
	pr_info("%s: unloaded\n",DRIVER_NAME);

}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("INTRO LOW RESOLUTION CODE");
MODULE_VERSION("1.0");

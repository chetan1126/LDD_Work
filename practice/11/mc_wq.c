// WaitQueue code

#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/uaccess.h>
#include<linux/wait.h>		//Provides wait queues APIs
#include<linux/sched.h>		//Task states and scheduling
#include<linux/slab.h>
#include<linux/mutex.h>
#include<linux/version.h>


#define DEVICE_NAME	"multi_wq"
#define CLASS_NAME	"wq_class"
#define BUFFER_SIZE	100
#define DEVICE_COUNT    4


static dev_t dev_number;
// static struct cdev wq_cdev;
static struct class *wq_class;
// static char device_buffer[BUFFER_SIZE];
// static int data_available = 0;	




struct multi_char_dev {
	struct cdev cdev;
	char device_buffer[BUFFER_SIZE];
	size_t data_size;

	struct mutex lock;	// Mutex lock for synchronizing access to this device
	int minor;		// Minor number of this device
	wait_queue_head_t wq;	//  <-- this is the type of wait queue head
	int data_available;
};


/********************************** READ *************************************/
static ssize_t wq_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
	int bytes_to_read;
	struct multi_char_dev *dev;
	dev = file->private_data;
	pr_info("multi_wq: read() called\n");


	/* 
	 * if data_available ==0, the proces sleeps here.
	 * 
	 * The process wakes up when:
	 * 1. wake_up_interruptible(&wq) is called in 'write file operation'
	 * 2. data is available (becomes non-zero)
	 * 3. signal interrupts the sleep
	 *
	 **/
	if(wait_event_interruptible(dev->wq, dev->data_available) != 0)
	{
		pr_info("multi_wq: read interrpted by signal\n");
		return -1;
	}

	/*
	 * Once the process wakes up, data is available
	 */

	bytes_to_read = min(count, strlen(dev->device_buffer) - (size_t)*offset);

	if(copy_to_user(user_buffer, dev->device_buffer+*offset, bytes_to_read))
	{
		pr_err("multi_wq: failed to copy data to user\n");
		return -EFAULT;
	}

	dev->data_available = 0;
	*offset = *offset + bytes_to_read;
	pr_info("multi_wq: sent %d bytes to user \n", bytes_to_read);
	return bytes_to_read;

}


/********************************** WRITE *************************************/
static ssize_t wq_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
	int bytes_to_write;
	struct multi_char_dev *dev;
	dev = file->private_data;
	pr_info("multi_wq: write() called");
	bytes_to_write = min(count, (size_t)(BUFFER_SIZE-1)- *offset);

	memset(dev->device_buffer, 0, BUFFER_SIZE);

	if(copy_from_user(dev->device_buffer+*offset, user_buffer, bytes_to_write))
	{
		pr_err("multi_wq: failed to copy data from user\n");
		return -EFAULT;	
	}

	*offset = *offset + bytes_to_write;

	dev->device_buffer[*offset] = '\0';

	dev->data_available = 1;
	//Wake up the process sleeping in read()
	wake_up_interruptible(&dev->wq);
	pr_info("multi_wq: received data %s\n", dev->device_buffer);
	pr_info("multi_wq: sleeping reader woken up\n");
	return bytes_to_write;

}

static int wq_open(struct inode *inode, struct file *file)
{
	struct multi_char_dev *dev ;
	dev = container_of(inode->i_cdev, struct multi_char_dev, cdev);

	file->private_data = dev;

	pr_info("multi_wq: open() called\n");
	return 0;

}

static int wq_release(struct inode *inode, struct file *file) 
{
	struct multi_char_dev *dev ;
	dev = file->private_data;
	
	pr_info("multi_wq: release() called\n");
	return 0;

}

// Pointer to dynamically allocated array of device structures
struct multi_char_dev *devices;



static struct file_operations wq_fops = {

	.owner	= THIS_MODULE,
	.open	= wq_open,
	.release= wq_release,
	.read 	= wq_read,
	.write 	= wq_write

};


static int __init wq_driver_init(void)
{
	int ret,i;
dev_t dev_num;
// dev_t base_dev;
	pr_info("multi_wq: Module loaded\n");

	ret = alloc_chrdev_region(&dev_number, 0, DEVICE_COUNT, DEVICE_NAME);	

	if(ret < 0)
	{
		pr_err("multi_wq: failed to allocate device number\n");
		return ret;
	}
	// pr_info("multi_wq: Major number = %d\n", MAJOR(base_dev));

	// pr_info("multi_wq: major = %d, minor = %d\n", MAJOR(dev_number), MINOR(dev_number));
	// cdev_init(&wq_cdev, &wq_fops);
	// ret = cdev_add(&wq_cdev, dev_number, 1);

	// if(ret < 0)
	// {
	// 	pr_err("multi_wq: failed to add cdev\n");
	// 	unregister_chrdev_region(dev_number, 1);
	// 	return ret;
	// }

	// Check if kernel version is 6.4.0 or newer
	// Check if kernel version is 6.4.0 or newer
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
		wq_class = class_create(CLASS_NAME);
	#else
		wq_class = class_create(THIS_MODULE, CLASS_NAME);
	
	#endif
	if(IS_ERR(wq_class))
	{
		pr_err("multi_wq: failed to create class\n");
		// cdev_del(&wq_cdev);
		unregister_chrdev_region(dev_number, DEVICE_COUNT);
		return PTR_ERR(wq_class);
	
	}

		devices = kcalloc(DEVICE_COUNT, sizeof(struct multi_char_dev), GFP_KERNEL);
		if(!devices)
		{
			ret = -ENOMEM; // Error NO memory
			class_destroy(wq_class);
			unregister_chrdev_region(dev_number, DEVICE_COUNT);
    		return ret;
		}

		for(i = 0; i < DEVICE_COUNT; ++i)
		{
			dev_num = MKDEV(MAJOR(dev_number), MINOR(dev_number)+i);
			devices[i].minor = i;
			devices[i].data_size = 0;
			devices[i].data_available = 0;
			mutex_init(&devices[i].lock);
			cdev_init(&devices[i].cdev, &wq_fops);

			init_waitqueue_head(&devices[i].wq);
			devices[i].cdev.owner = THIS_MODULE;
			ret = cdev_add(&devices[i].cdev, dev_num, 1);
			if(ret<0)
			{
				pr_err("multi_wq: cdev_add failed for minor %d\n", i);
				while(--i >= 0)
				{
					dev_num = MKDEV(MAJOR(dev_number),MINOR(dev_number) +i);
						device_destroy(wq_class, dev_num);
						cdev_del(&devices[i].cdev);
				}
				kfree(devices);	
				return ret;
			}

			if(IS_ERR(device_create(wq_class,
				 NULL, 
				 dev_num,
				  NULL, 
				  "multi_wq%d",
				  i)))
			{
			pr_err("multi_wq: device create failed for minor %d\n", i);
				class_destroy(wq_class);
				cdev_del(&devices[i].cdev);
				// unregister_chrdev_region(dev_number, 1);
				while(--i >= 0)
				{
					dev_num = MKDEV(MAJOR(dev_number),MINOR(dev_number) +i);
						device_destroy(wq_class, dev_num);
						cdev_del(&devices[i].cdev);
				}
				kfree(devices);	
				return -ERESTARTSYS;

			}
		pr_info("multi_wq: created /dev/multi_wq%d\n", i);

		} // end of for loop




	pr_info("multi_wq: device created at /dev/%s\n", DEVICE_NAME);
	return 0;


}

static void __exit wq_driver_exit(void)
{
	int i;
	dev_t dev_num;

	for (i = 0; i < DEVICE_COUNT; ++i) {
		dev_num = MKDEV(MAJOR(dev_number), MINOR(dev_number) + i);
		device_destroy(wq_class, dev_num);
		cdev_del(&devices[i].cdev);
		pr_info("multi_wq: removed /dev/multi_wq%d", i);

	}
	kfree(devices);
	class_destroy(wq_class);
	unregister_chrdev_region(dev_number, DEVICE_COUNT);
	pr_info("multi_wq: module unloaded\n");
}



module_init(wq_driver_init);
module_exit(wq_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chetan");
MODULE_DESCRIPTION("Wait Queue multiple device driver");
MODULE_VERSION("1.0");

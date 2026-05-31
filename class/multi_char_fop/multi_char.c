// 1. Code for Multiple character devices

#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/cdev.h>
// Include device model APIs such as class_creat() and device_creat()
#include<linux/device.h>
// Include user kernel copy function copy_to_user() and copy_from_user()
#include<linux/uaccess.h>
// Include kernel memory allocation functions such as kcalloc() and kfree()
#include<linux/slab.h>
#include<linux/mutex.h>
// Include kernel version macros such as LINUX_VERSION_CODE and KERNEL_VERSION()
#include<linux/version.h>

// Define the base class while allocating character device number
#define DEVICE_NAME	"multi_char"
// Define the device class name visible under /sys/class
#define CLASS_NAME	"multi_char_class"
#define DEVICE_COUNT    4
#define BUFFER_SIZE     1024

//static char kernel_buffer[]= "Hello Everyone. My name is Chetan Kotrange. Welcome to Linux Device Driver CLass space\n";

// Define a structure that represents one character device instance
struct multi_char_dev{
	struct cdev cdev;// Character device object registered with VFS layer
	char buffer[BUFFER_SIZE];// Private kernel buffer for this particular device
	size_t data_size;	// Number of valid bytes currently stored in this device buffer
	struct mutex lock;// Mutex lock
	int minor;	//Minor number of this device
};

// Stores first allocated device number. This also contains both major and minor number.
static dev_t base_dev;

// Pointer to device class used for automatic /dev node creation.
struct class *multi_char_class;

// Pointer to dynamically allocated array of device structures
struct multi_char_dev *devices;

//**************************OPEN****************************************
static int multi_char_open(struct inode *inode, struct file *file)
{
	//declare a pointer to our device specific structure
	struct multi_char_dev *dev;

	// Get the address of the parent structure from cdev pointer.
	// inode->i_cdev points to the cdev of the opened device
	// container_of() gives the full struct 'multi_char_dev' containing the cdev
	
	dev = container_of(inode->i_cdev, struct multi_char_dev, cdev);

	// Store the device pointer inside file->private_data 
	// Later read(), write(), release(), lseek() can use this pointer
	file->private_data = dev;
	pr_info("multi_char: open called for minor%d\n", dev->minor);
	return 0;

}

/**************************RELEASE***************************************/

static int multi_char_release(struct inode *inode, struct file *file)
{
	// get device pointer that was stored during open()
	struct multi_char_dev *dev = file->private_data;

	pr_info("multi_char: release called for minor %d\n", dev->minor);

	return 0;
}


/**************************READ****************************************/

static ssize_t multi_char_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
	// Get device pointer from file->private_data.
	// This tells us which minor device is being used.
	struct multi_char_dev *dev = file->private_data;

	size_t available;
	size_t bytes_to_read;
	size_t ret;

	mutex_lock(&dev->lock);

	if(*offset >= dev->data_size)
	{
		ret = 0;
		mutex_unlock(&dev->lock);
		return ret;
	}

	available = dev->data_size - (size_t)(*offset);

	bytes_to_read = min(count, available);

	if(copy_to_user(user_buffer, dev->buffer + *offset, bytes_to_read))
	{
		ret = -EFAULT;

		mutex_unlock(&dev->lock);
		return ret;
	}
	pr_info("multi_char: Read Data = %.*s\n",(int)bytes_to_read,dev->buffer+ *offset);
	*offset += bytes_to_read;

	ret = bytes_to_read;
	pr_info("multi_char:read: read %zu bytes from minor %d\n", bytes_to_read, dev->minor);
	
	mutex_unlock(&dev->lock);
	return ret;

}

/**************************WRITE****************************************/
static ssize_t multi_char_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
	
	struct multi_char_dev *dev = file->private_data;
	size_t space_left;
	ssize_t bytes_to_write;
	size_t ret;

	mutex_lock(&dev->lock);
	space_left = BUFFER_SIZE - (size_t) (*offset);

	bytes_to_write = min(count, space_left);
	
	if(copy_from_user(dev->buffer+*offset, user_buffer, bytes_to_write))
	{
		ret = -EFAULT;

		mutex_unlock(&dev->lock);
		return ret;
	
	}
	if(*offset >= BUFFER_SIZE)
	{
		ret = -ENOSPC;
		mutex_unlock(&dev->lock);
		return ret;lsmod | grep multi
cat /proc/devices | grep multi
	}
	pr_info("multi_char:write: Written Data = %s\n", dev->buffer);
	*offset += bytes_to_write;

	if(*offset > dev->data_size)
		dev->data_size = *offset;

	
	LINUX_VERSION_CODE
	ret = bytes_to_write;
	pr_info("multi_char: wrote %zu bytes to minor %d\n", bytes_to_write, dev->minor);

	mutex_unlock(&dev->lock);
	return ret;
}

/************************** llseek ****************************************/
static loff_t multi_char_llseek(struct file *file, loff_t offset, int whence)
{
	struct multi_char_dev *dev = file->private_data;

	loff_t new_pos;

	mutex_lock(&dev->lock);

	switch(whence)
	{
		case SEEK_SET:
		     new_pos = offset;
		     break;

		case SEEK_CUR:
		     new_pos = file->f_pos + offset;
		     break;

		case SEEK_END:
		     new_pos = dev->data_size + offset;
		     break;

		default:
		     mutex_unlock(&dev->lock);
		     return -EINVAL;

	}

	if(new_pos < 0 || new_pos > BUFFER_SIZE)
	{
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	file->f_pos = new_pos;
	mutex_unlock(&dev->lock);
	return new_pos;
}

static const struct file_operations multi_char_fops = {
	.owner = THIS_MODULE,
	.open = multi_char_open,
	.release = multi_char_release,
	.read = multi_char_read,
	.write = multi_char_write,
	.llseek = multi_char_llseek
};

static int __init multi_char_init(void)
{
	
	int ret;
	int i;
	dev_t dev_num;	/* 32-bit ulsmod | grep multi
cat /proc/devices | grep multinsigned int (12-bit: Major & 20 bits: Minor) */
	pr_info("multi_char: module init\n");

	// Function for Dynamic allocation
	ret = alloc_chrdev_region(&base_dev, 0, DEVICE_COUNT, DEVICE_NAME);

	if(ret < 0)
	{
		pr_err("multi_char: failed to allocated number\n");
		return ret;
	}
	pr_info("multi_char: Major number = %d\n", MAJOR(base_dev));

	// Check if kernel version is 6.4.0 or newer
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
		multi_char_class = class_create(CLASS_NAME);
	#else
		multi_char_class = class_create(THIS_MODULE, CLASS_NAME);
	
	#endif

	//Check wheather class creation failed
	if(IS_ERR(multi_char_class))
	{
		pr_err("multi_char: failed to create class\n");
		// Converts error pointer into error number(int)
		ret = PTR_ERR(multi_char_class);

		goto unregister_region;

	}

	// Allocate zero-initialized memory for DEVICE_COUNT device structure 
	devices = kcalloc(DEVICE_COUNT, sizeof(struct multi_char_dev), GFP_KERNEL);
	
	if(!devices)
	{
		ret = -ENOMEM; // Error NO MEMory
		goto destroy_class;
	}

	// Loop through all minor devices
	for(i = 0; i < DEVICE_COUNT; ++i)
	{
		// Create device number using allocated major and minor number i
		dev_num = MKDEV(MAJOR(base_dev), MINOR(base_dev)+i);

		// Store minor number inside device structure
		devices[i].minor = i;

		// Initially no valid data present in the buffer
		devices[i].data_size = 0;
	
		//  Initialize mutex lock for this device
		mutex_init(&devices[i].lock);

		cdev_init(&devices[i].cdev, &multi_char_fops);
		
		devices[i].cdev.owner = THIS_MODULE;

		ret = cdev_add(&devices[i].cdev, dev_num, 1);

		// Check 
		if(ret<0)
		{
			pr_err("multi_char: cdev_add failed for minor %d\n", i);
			goto cleanup_devices;
		}
		
		// Create /dev/multi_charX device node
		if(IS_ERR(device_create(multi_char_class,
						NULL,
						dev_num,
						NULL,
						"multi_char%d",
						i)))
		{
			pr_err("multi_char: device create failed for minor %d\n", i);
			cdev_del(&devices[i].cdev);
			ret = -EINVAL;
			goto cleanup_devices;
		}
		pr_info("multi_char: created /dev/multi_char%d\n", i);

	} // end of for loop

	return 0;

cleanup_devices:
	while(--i >= 0)
	{
		dev_num = MKDEV(MAJOR(base_dev),MINOR(base_dev) +i);
	        device_destroy(multi_char_class, dev_num);
       		cdev_del(&devices[i].cdev);
 	}
	kfree(devices);	

destroy_class:
	class_destroy(multi_char_class);

unregister_region:
	unregister_chrdev_region(base_dev, DEVICE_COUNT);
	return ret;

}

static void __exit multi_char_exit(void)
{
	int i;

	dev_t dev_num;

	pr_info("multi_char: module exit\n");

	for(i = 0; i < DEVICE_COUNT; ++i)
	{
		// create device number using allocated major and minor numbrs i
		dev_num = MKDEV(MAJOR(base_dev), MINOR(base_dev) + i);
	
		device_destroy(multi_char_class, dev_num);

		cdev_del(&devices[i].cdev);
		pr_info("multi_char: removed /dev/multi_char%d", i);

	}
	kfree(devices);
	class_destroy(multi_char_class);
	unregister_chrdev_region(base_dev, DEVICE_COUNT);
}



module_init(multi_char_init);
module_exit(multi_char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chetan");
MODULE_DESCRIPTION("Multiple character devices using one driver and multiple minors numbers\n");
MODULE_VERSION("1.0");




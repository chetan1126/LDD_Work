#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/kdev_t.h>
#include "ioctl_cmd.h"

#define BUFFER_SIZE	100
#define DEVICE_NAME 	"file_operations"
#define DEVICE_COUNT	1

static char kernel_buffer[BUFFER_SIZE] = "HELLO FROM LINUX KERNEL DRIVER\n";
static const char device_buffer [] = "Hello everyone. My name is Neel Madhav. Welcome to Linux Device Driver\n";
static dev_t dev_number;
static struct cdev char_cdev;
static int device_value;

#define DEVICE_SIZE (sizeof(device_buffer) - 1)

static int my_open(struct inode *inode, struct file *file)
{
	pr_info("file operations demo\n");
	pr_info("MAJOR = %d, MINOR = %d\n", imajor(inode), iminor(inode));
	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
	pr_info("Relased Called\n");
	return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
	int bytes_to_read;
	pr_info("Read Function Called\n");

	if(*offset >=strlen(device_buffer))
	{
		pr_info("End of File Reached\n");
		return 0;
	}

	bytes_to_read = min(count, strlen(device_buffer) - (size_t) *offset);

	if(copy_to_user(user_buffer, device_buffer+*offset, bytes_to_read) != 0)
	{
		pr_err("Failed to copy Data to USer \n");
		return -EFAULT;
	}

	*offset += bytes_to_read;
	pr_info("sent %d bytes to user \n", bytes_to_read);
	return bytes_to_read;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
	size_t available_space;
	size_t bytes_to_write;
	pr_info("Write Fucntion Called\n");
	
	if(*offset >= BUFFER_SIZE - 1)
	{
		pr_err("No space left in Buffer\n");
		return -ENOSPC;
	}	
	available_space = (BUFFER_SIZE - 1) - *offset;

	bytes_to_write = min(count, available_space);
	pr_info("Requested = %zu bytes \n", count);
	pr_info("Writing = %zu bytes \n", bytes_to_write);
	if(copy_from_user(kernel_buffer+*offset, user_buffer, bytes_to_write) !=  0)
	{
		pr_err("Failed to copy data from user \n");
		return -EFAULT;
	}

	*offset += bytes_to_write;
	kernel_buffer[*offset] = '\0';
	pr_info("Kernel buffer = %s\n", kernel_buffer);
	pr_info("Updated offset = %lld\n", *offset);
	return bytes_to_write;

}

static loff_t lseek_llseek (struct file *filp, loff_t offset, int whence)
{
	loff_t new_pos;

	switch (whence)
	{
		case SEEK_SET:
			new_pos = offset;
			break;

		case SEEK_CUR:
			new_pos = filp->f_pos + offset;
			break;

		case SEEK_END:
			new_pos = DEVICE_SIZE + offset;
			break;

		default:
			return -EINVAL;
	}

	if(new_pos < 0 || new_pos > DEVICE_SIZE)
		return -EINVAL;

	filp -> f_pos = new_pos;
	pr_info("lseek_char: new file position = %lld\n", new_pos);

	return new_pos;
}

static long my_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	int value;
	pr_info("ioctl_char: ioctl called\n");
	switch(cmd)
	{
		case MY_IOCTL_RESET:
			device_value = 0;
			pr_info("ioctl_char: device_value reset to 0\n");
			break;
		case MY_IOCTL_SET_VALUE:
			if(copy_from_user(&value, (int __user*) arg, sizeof(value)))
			{
				pr_err("ioctl_char: failed to copy value from user\n");
				return -EFAULT;
			}
			device_value = value;
			pr_info("ioctl_char: device_value set to %d\n", device_value);
			break;
		case MY_IOCTL_GET_VALUE:
			value = device_value;
			if(copy_to_user((int __user*) arg, &value, sizeof(value)))
			{
				pr_err("ioclt_char: device_value sent to user: %d\n", value);
				break;
			}
			break;
		case MY_IOCTL_TOGGLE_VALUE:
			device_value = !device_value;
			pr_info("ioctl_char: device_value toggled to %d\n", device_value);
			break;
		default:
			pr_err("ioctl_char : invalid ioctl command\n");
			return -EINVAL;
	}
	return 0;
}

static const struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release,
	.read = my_read,
	.write = my_write,
	.llseek = lseek_llseek,
	.unlocked_ioctl = my_ioctl
};

static int __init my_init(void)
{
	int ret;
	pr_info("Character module loading\n");

	ret = alloc_chrdev_region(&dev_number, 0, DEVICE_COUNT, DEVICE_NAME);
	if( ret < 0)
	{
		pr_err("file_operations: failed to dynamically allocate device number\n");
		return ret;
	}

	pr_info("file_operations: allocated MAJOR = %d, MINOR = %d\n", MAJOR(dev_number), MINOR(dev_number));

	cdev_init(&char_cdev, &fops);
	
	ret =  cdev_add(&char_cdev, dev_number, DEVICE_COUNT);
	if(ret < 0)
	{
		pr_err("file_operations: failed to add cdev\n");
		unregister_chrdev_region(dev_number, DEVICE_COUNT);
		return ret;
	}
	
	pr_info("file_operations: cdev added successfully\n");
	return 0;	
}

static void __exit my_exit(void)
{
	pr_info("file_operations: module_loading\n");
	cdev_del(&char_cdev);
	unregister_chrdev_region(dev_number, DEVICE_COUNT);
	pr_info("file_operations: module unloaded\n");

}


module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");


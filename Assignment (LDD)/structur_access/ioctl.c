#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include"ioctl.h"

#define	DEVICE_NAME	"char_demo"
#define	DEVICE_COUNT	1
//#define	buffer_size	1000
static dev_t dev_number;
static struct cdev char_dev;

static char kernel_buffer[]= "Hello Everyone. My name is Chetan Kotrange. Welcome to Linux Device Driver CLass space\n";


#define	buffer_size	(strlen(kernel_buffer))
#define	DEVICE_SIZE	(sizeof(kernel_buffer)-1)

static int device_value;
static dev_t dev_number;
static struct cdev char_dev;

static struct my_struct
{
	int data;
}my_st;



static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int value;
	//struct my_struct my_st;
	my_st.data = 99;
	
	pr_info("ioctl_fun: ioctl called\n");
	
	switch(cmd)
	{
		case MY_IOCTL_RESET: 
					device_value =0;
					pr_info("ioctl_fun:reset: device value reset to zero(0)\n value=%d\n",value);
					break;
		case MY_IOCTL_SET_VALUE:
					if(copy_from_user(&value, (int __user*)arg, sizeof(value)))
					{
						pr_err("ioctl_fun:set: device_value sent to user: %d\n", value);
						return -EFAULT;
					}
					device_value = value;
					pr_info("ioctl_fun:set: device value set to %d\n", device_value);
					break;
		case MY_IOCTL_GET_VALUE:
					value = device_value;
					if(copy_to_user((int __user*)arg,&value,sizeof(value)))
					{
						pr_err("ioctl_fun:get value: failed to sent data to user\n");
						return -EFAULT;
					}
					pr_err("ioctl_fun:get_value: call successful\nvalue=%d\n",value);
					break;
		case MY_IOCTL_TOGGLE_VALUE:
						 device_value =! device_value;
						pr_info("ioctl_fun:toggle_value: device_value toggled to %d\n",device_value);
						break;
		case MY_IOCTL_GET_STRUCT:
					//value = my_st.data;
					if(copy_to_user((void __user*)arg,&my_st, sizeof(my_st)))
					{
						pr_err("ioctl_fun:get_struct: failed to sent data to user\n");
						return -EFAULT;
					}
					pr_err("ioctl_fun:get_struct: call successful\nvalue=%d\n",value);
					break;
		default:
			pr_err("ioctl_fun: invalid ioctl command\n");
			return -EINVAL;	
	}
	return 0;
}

static loff_t my_lseek(struct file *file_ptr, loff_t offset, int whence)
{
	loff_t new_pos;
	
	switch(whence)
	{
		case SEEK_SET: new_pos =  offset;
			break;
		case SEEK_CUR: new_pos = (file_ptr->f_pos) + offset;
			break;
		case SEEK_END: new_pos = DEVICE_SIZE + offset;
			break;
		default: return -EINVAL;
	}

	if(new_pos <0 || new_pos> DEVICE_SIZE) return -EINVAL;

	file_ptr->f_pos = new_pos;
	pr_info("lseek_char: new file position= %lld\n", new_pos);
	return new_pos;

}

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

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
	int bytes_to_read;
	
	pr_info("Read_fun: Read function called\n");
	
	if(*offset >= strlen(kernel_buffer))
	{
		pr_info("Read_fun: End of file reached\n");
		return 0;
	}
	
	bytes_to_read = min(count, (strlen(kernel_buffer) - (size_t) *offset));
	
	if(copy_to_user(user_buffer, kernel_buffer+*offset, bytes_to_read) != 0)
	{
		pr_err("Read_fun: failed to copy data to user\n");
		return -EFAULT;
	}
	
	*offset = *offset + bytes_to_read;
	pr_info("Read_fun: Sent %d bytes to the user\n", bytes_to_read);
	return bytes_to_read;
	
}
static ssize_t my_write(struct file *file, const char __user *user_buffer,size_t count, loff_t *offset)
{
	int bytes_to_write;
	int ret;
	pr_info("Write_fun: Write function called\n");
	if(*offset > buffer_size)
	{
		pr_info("Write_fun: End of file space\n");
		return -ENOSPC;
	}
	
	bytes_to_write = min(count, (size_t)((buffer_size-1) - *offset));
	
	 
	ret = copy_from_user(kernel_buffer+*offset, user_buffer, bytes_to_write);
	
	if(ret != 0)
	{
		pr_err("Write_fun: failed to copy data from user\n");
		return -EFAULT;
	}
	
	*offset += bytes_to_write;
	
	kernel_buffer[*offset] = '\0';
	pr_info("Write_fun: recived %d bytes from the user\n", bytes_to_write);
	pr_info("Write_fun: recived string = %s\n", kernel_buffer);
	return bytes_to_write;
}

static const struct file_operations fops =
{	
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release,
	.read = my_read,
	.write = my_write,
	.llseek = my_lseek,
	.unlocked_ioctl = my_ioctl	
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h> 

#define BUFFER_SIZE     100
#define DEVICE_NAME     "semaphore_dev"
#define DEVICE_COUNT    1

static const char device_buffer[] = "Hello dosto, Namaskar from priyanikita!\n";
static dev_t dev_number;
static struct cdev char_cdev;

static struct semaphore sem;

static int my_open(struct inode *inode, struct file *file)
{
    pr_info("%s: Process trying to open device [MAJOR = %d, MINOR = %d]\n", 
            DEVICE_NAME, imajor(inode), iminor(inode));

    if (down_interruptible(&sem)) {
        pr_warn("%s: Open interrupted while waiting for semaphore slot\n", DEVICE_NAME);
        return -ERESTARTSYS;
    }

    pr_info("%s: Device opened successfully. Slot acquired.\n", DEVICE_NAME);
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    pr_info("%s: Close called. Releasing slot.\n", DEVICE_NAME);
    up(&sem);
    
    return 0;
}
 
static ssize_t my_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    int bytes_to_read;

    if (*offset >= strlen(device_buffer))
    {
        return 0;
    }

    bytes_to_read = min(count, strlen(device_buffer) - (size_t)*offset);

    if (copy_to_user(user_buffer, device_buffer + *offset, bytes_to_read) != 0)
    {
        return -EFAULT;
    }

    *offset += bytes_to_read;
    return bytes_to_read;
}
static const struct file_operations fops = 
{
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
};

static int __init my_init(void)
{
    int ret;
    pr_info("%s: Loading semaphore module...\n", DEVICE_NAME);
    ret = alloc_chrdev_region(&dev_number, 0, DEVICE_COUNT, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("%s: Failed to dynamically allocate device number\n", DEVICE_NAME);
        return ret;
    }
    cdev_init(&char_cdev, &fops);
    
    ret = cdev_add(&char_cdev, dev_number, DEVICE_COUNT);
    if (ret < 0)
    {
        pr_err("%s: Failed to add cdev\n", DEVICE_NAME);
        unregister_chrdev_region(dev_number, DEVICE_COUNT);
        return ret;
    }
    sema_init(&sem, 3);
    
    pr_info("%s: Initialized successfully. Major: %d, Minor: %d\n", 
            DEVICE_NAME, MAJOR(dev_number), MINOR(dev_number));
    return 0;   
}

static void __exit my_exit(void)
{
    cdev_del(&char_cdev);
    unregister_chrdev_region(dev_number, DEVICE_COUNT);
    pr_info("%s: Module unloaded cleanly\n", DEVICE_NAME);
}

module_init(my_init);
module_exit(my_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("limit device access to 3 users  using a semaphore");



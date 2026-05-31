#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/mutex.h> 

#define BUFFER_SIZE     1024
#define DEVICE_NAME     "mutex_dev"
#define DEVICE_COUNT    1

static char kernel_buffer[BUFFER_SIZE];
static size_t actual_data_size = 0; 
static struct mutex buffer_lock;

static dev_t dev_number;
static struct cdev char_cdev;

static int my_open(struct inode *inode, struct file *file)
{
    pr_info("%s: Device opened successfully\n", DEVICE_NAME);
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    pr_info("%s: Device released successfully\n", DEVICE_NAME);
    return 0;
}
static ssize_t my_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    int bytes_to_read;
    pr_info("%s: Read function called\n", DEVICE_NAME);

    if (mutex_lock_interruptible(&buffer_lock)) 
    {
        return -EINVAL;
    }

    if (*offset >= actual_data_size) {
        pr_info("%s: End of file reached during read\n", DEVICE_NAME);
        mutex_unlock(&buffer_lock);
        return 0;
    }

    bytes_to_read = min(count, actual_data_size - (size_t)*offset);

    if (copy_to_user(user_buffer, kernel_buffer + *offset, bytes_to_read) != 0) {
        pr_err("%s: Failed to copy data to user space\n", DEVICE_NAME);
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    *offset += bytes_to_read;
    pr_info("%s: Sent %d bytes to user. New offset: %lld\n", DEVICE_NAME, bytes_to_read, *offset);

    mutex_unlock(&buffer_lock);
    return bytes_to_read;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
    size_t bytes_to_write;
    pr_info("%s: Write function called\n", DEVICE_NAME);

    if (mutex_lock_interruptible(&buffer_lock)) {
        return -EINVAL;
    }
    bytes_to_write = min(count, (size_t)(BUFFER_SIZE - 1));
 
    if (copy_from_user(kernel_buffer, user_buffer, bytes_to_write) != 0) {
        pr_err("%s: Failed to copy data from user space\n", DEVICE_NAME);
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    actual_data_size = bytes_to_write;
    kernel_buffer[actual_data_size] = '\0'; 
    *offset = bytes_to_write;

    pr_info("%s: Kernel buffer updated: %s", DEVICE_NAME, kernel_buffer);
    pr_info("%s: Received %zu bytes from user\n", DEVICE_NAME, bytes_to_write);

    mutex_unlock(&buffer_lock);
    return bytes_to_write;
}

static const struct file_operations fops = 
{
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .write   = my_write,
};

static int __init my_init(void)
{
    int ret;
    pr_info("%s: Loading module...\n", DEVICE_NAME);
    ret = alloc_chrdev_region(&dev_number, 0, DEVICE_COUNT, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("%s: Failed to allocate device number\n", DEVICE_NAME);
        return ret;
    }
    pr_info("%s: Allocated MAJOR = %d, MINOR = %d\n", DEVICE_NAME, MAJOR(dev_number), MINOR(dev_number));

    cdev_init(&char_cdev, &fops);
    
    ret = cdev_add(&char_cdev, dev_number, DEVICE_COUNT);
    if (ret < 0) 
    {
        pr_err("%s: Failed to add cdev\n", DEVICE_NAME);
        unregister_chrdev_region(dev_number, DEVICE_COUNT);
        return ret;
    }

    mutex_init(&buffer_lock);
    
    pr_info("%s: Module initialized successfully\n", DEVICE_NAME);
    return 0;   
}

static void __exit my_exit(void)
{
    pr_info("%s: Unloading module\n", DEVICE_NAME);
    
    cdev_del(&char_cdev);
    unregister_chrdev_region(dev_number, DEVICE_COUNT);
    
    pr_info("%s: Module unloaded cleanly\n", DEVICE_NAME);
}

module_init(my_init);
module_exit(my_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("protect shared device buffer using mutex");



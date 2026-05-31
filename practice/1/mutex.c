// mutex_driver.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DEVICE_NAME "mutex_dev"
#define BUF_SIZE 1024

static dev_t dev_num;
static struct cdev mutex_cdev;
static struct class *mutex_class;

static char kernel_buffer[BUF_SIZE];
static int data_size = 0;

/* Mutex */
static struct mutex buffer_lock;

/* Open */
static int mutex_open(struct inode *inode, struct file *file)
{
    pr_info("Device Opened\n");
    return 0;
}

/* Release */
static int mutex_drv_release(struct inode *inode, struct file *file)
{
    pr_info("Device Closed\n");
    return 0;
}

/* Write */
static ssize_t mutex_write(struct file *file,
                           const char __user *buf,
                           size_t len,
                           loff_t *off)
{
    if (len > BUF_SIZE)
        len = BUF_SIZE;

    mutex_lock(&buffer_lock);

    if (copy_from_user(kernel_buffer, buf, len))
    {
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    data_size = len;

    pr_info("Data Written: %d bytes\n", data_size);

    mutex_unlock(&buffer_lock);

    return len;
}

/* Read */
static ssize_t mutex_read(struct file *file,
                          char __user *buf,
                          size_t len,
                          loff_t *off)
{
    int bytes_to_read;

    if (*off >= data_size)
        return 0;

    mutex_lock(&buffer_lock);

    bytes_to_read = min((int)len, data_size);

    if (copy_to_user(buf, kernel_buffer, bytes_to_read))
    {
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    *off += bytes_to_read;

    mutex_unlock(&buffer_lock);

    return bytes_to_read;
}

static struct file_operations fops =
{
    .owner   = THIS_MODULE,
    .open    = mutex_open,
    .read    = mutex_read,
    .write   = mutex_write,
    .release = mutex_drv_release,
};

/* Module Init */
static int __init mutex_driver_init(void)
{
    alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    pr_info("Device Registered: Major %d, Minor %d\n", MAJOR(dev_num), MINOR(dev_num));
    cdev_init(&mutex_cdev, &fops);
    cdev_add(&mutex_cdev, dev_num, 1);

    mutex_class = class_create(DEVICE_NAME);

    device_create(mutex_class,
                  NULL,
                  dev_num,
                  NULL,
                  DEVICE_NAME);

    mutex_init(&buffer_lock);

    pr_info("Mutex Driver Loaded\n");
    return 0;
}

/* Module Exit */
static void __exit mutex_driver_exit(void)
{
    device_destroy(mutex_class, dev_num);
    class_destroy(mutex_class);

    cdev_del(&mutex_cdev);
    unregister_chrdev_region(dev_num, 1);

    pr_info("Mutex Driver Unloaded\n");
}

module_init(mutex_driver_init);
module_exit(mutex_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chetan");
MODULE_DESCRIPTION("Character Driver using Mutex");

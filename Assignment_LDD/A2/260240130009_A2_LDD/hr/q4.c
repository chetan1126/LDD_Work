#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/atomic.h>

#define DEVICE_NAME "hrtimer_dev"
#define DEVICE_COUNT 1
#define TIMER_INTERVAL_MS 500 
static dev_t dev_number;
static struct cdev char_cdev;

static struct hrtimer my_hrtimer;
static ktime_t interval;
static atomic_t global_counter = ATOMIC_INIT(0);
static enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
{
    atomic_inc(&global_counter);
    hrtimer_forward_now(timer, interval);
    return HRTIMER_RESTART;
}
static int my_open(struct inode *inode, struct file *file)
{
    pr_info("%s: Device opened\n", DEVICE_NAME);
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    pr_info("%s: Device closed\n", DEVICE_NAME);
    return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    char local_buf[64];
    int len;
    int current_count;

  
    if (*offset > 0)
    {
        return 0;
    }

    current_count = atomic_read(&global_counter);
    len = scnprintf(local_buf, sizeof(local_buf), "Timer Ticks Count: %d\n", current_count);

    if (count < len)
    {
       return -EINVAL;
    }
    if (copy_to_user(user_buffer, local_buf, len) != 0) 
    {
        pr_err("%s: copy_to_user failed\n", DEVICE_NAME);
        return -EFAULT;
    }
    *offset += len;

    return len;
}

static const struct file_operations fops =
{
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
};

static int __init hrtimer_dev_init(void)
{
    int ret;
    pr_info("%s: Initializing module\n", DEVICE_NAME);

    ret = alloc_chrdev_region(&dev_number, 0, DEVICE_COUNT, DEVICE_NAME);
    if (ret < 0) 
    {
        pr_err("%s: Failed to allocate major number\n", DEVICE_NAME);
        return ret;
    }

    cdev_init(&char_cdev, &fops);
    ret = cdev_add(&char_cdev, dev_number, DEVICE_COUNT);
    if (ret < 0)
    {
        pr_err("%s: Failed to register cdev\n", DEVICE_NAME);
        unregister_chrdev_region(dev_number, DEVICE_COUNT);
        return ret;
    }

    interval = ms_to_ktime(TIMER_INTERVAL_MS);
    hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    my_hrtimer.function = hrtimer_callback;
    
    hrtimer_start(&my_hrtimer, interval, HRTIMER_MODE_REL);

    pr_info("%s: Driver loaded. Major: %d, Minor: %d\n", DEVICE_NAME, MAJOR(dev_number), MINOR(dev_number));
    return 0;
}

static void __exit hrtimer_dev_exit(void)
{
    pr_info("%s: Unloading module\n", DEVICE_NAME);
    hrtimer_cancel(&my_hrtimer);
    cdev_del(&char_cdev);
    unregister_chrdev_region(dev_number, DEVICE_COUNT);

    pr_info("%s: Driver completely unloaded\n", DEVICE_NAME);
}

module_init(hrtimer_dev_init);
module_exit(hrtimer_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("accurate periodic event using high resolution timer");



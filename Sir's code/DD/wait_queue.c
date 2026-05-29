// waitqueue_demo.c

#include <linux/init.h>        // module init and exit macros
#include <linux/module.h>      // required for kernel modules
#include <linux/kernel.h>      // pr_info(), pr_err()
#include <linux/fs.h>          // file_operations, alloc_chrdev_region()
#include <linux/cdev.h>        // cdev structure and APIs
#include <linux/device.h>      // class_create(), device_create()
#include <linux/uaccess.h>     // copy_to_user(), copy_from_user()
#include <linux/wait.h>        // wait queue APIs
#include <linux/sched.h>       // task states and scheduling

#define DEVICE_NAME "wq_demo"
#define CLASS_NAME  "wq_class"
#define BUFFER_SIZE 100

static dev_t dev_number;
static struct cdev wq_cdev;
static struct class *wq_class;

static char device_buffer[BUFFER_SIZE];

static int data_available = 0;

static DECLARE_WAIT_QUEUE_HEAD(wq);

static ssize_t wq_read(struct file *file,
                       char __user *user_buffer,
                       size_t count,
                       loff_t *offset)
{
    int bytes_to_read;

    pr_info("wq_demo: read() called\n");

    /*
     * If data_available == 0, the process sleeps here.
     *
     * The process wakes up when:
     * 1. wake_up_interruptible(&wq) is called
     * 2. data_available becomes non-zero
     * 3. signal interrupts the sleep
     */
    if (wait_event_interruptible(wq, data_available != 0)) {
        pr_info("wq_demo: read interrupted by signal\n");
        return -ERESTARTSYS;
    }

    /*
     * Once the process wakes up, data is available.
     */
    bytes_to_read = min(count, strlen(device_buffer));

    if (copy_to_user(user_buffer, device_buffer, bytes_to_read)) {
        pr_err("wq_demo: failed to copy data to user\n");
        return -EFAULT;
    }

    /*
     * Mark data as consumed.
     * Next read will sleep again until new data is written.
     */
    data_available = 0;

    pr_info("wq_demo: sent %d bytes to user\n", bytes_to_read);

    return bytes_to_read;
}

static ssize_t wq_write(struct file *file,
                        const char __user *user_buffer,
                        size_t count,
                        loff_t *offset)
{
    int bytes_to_write;

    pr_info("wq_demo: write() called\n");

    bytes_to_write = min(count, (size_t)(BUFFER_SIZE - 1));

    memset(device_buffer, 0, BUFFER_SIZE);

    if (copy_from_user(device_buffer, user_buffer, bytes_to_write)) {
        pr_err("wq_demo: failed to copy data from user\n");
        return -EFAULT;
    }

    device_buffer[bytes_to_write] = '\0';

    /*
     * Data is now available.
     */
    data_available = 1;

    /*
     * Wake up any process sleeping in read().
     */
    wake_up_interruptible(&wq);

    pr_info("wq_demo: received data: %s\n", device_buffer);
    pr_info("wq_demo: sleeping reader woken up\n");

    return bytes_to_write;
}

static int wq_open(struct inode *inode, struct file *file)
{
    pr_info("wq_demo: open() called\n");
    return 0;
}

static int wq_release(struct inode *inode, struct file *file)
{
    pr_info("wq_demo: release() called\n");
    return 0;
}

static struct file_operations wq_fops = {
    .owner   = THIS_MODULE,
    .open    = wq_open,
    .release = wq_release,
    .read    = wq_read,
    .write   = wq_write,
};

static int __init wq_driver_init(void)
{
    int ret;

    pr_info("wq_demo: module loaded\n");

    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("wq_demo: failed to allocate device number\n");
        return ret;
    }

    pr_info("wq_demo: major = %d, minor = %d\n",
            MAJOR(dev_number), MINOR(dev_number));

    cdev_init(&wq_cdev, &wq_fops);

    ret = cdev_add(&wq_cdev, dev_number, 1);
    if (ret < 0) {
        pr_err("wq_demo: failed to add cdev\n");
        unregister_chrdev_region(dev_number, 1);
        return ret;
    }

    /*
     * For Linux 6.4+:
     *     class_create(CLASS_NAME)
     *
     * For older kernels such as 5.10:
     *     class_create(THIS_MODULE, CLASS_NAME)
     */
    wq_class = class_create(CLASS_NAME);
    if (IS_ERR(wq_class)) {
        pr_err("wq_demo: failed to create class\n");
        cdev_del(&wq_cdev);
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(wq_class);
    }

    if (IS_ERR(device_create(wq_class, NULL, dev_number, NULL, DEVICE_NAME))) {
        pr_err("wq_demo: failed to create device\n");
        class_destroy(wq_class);
        cdev_del(&wq_cdev);
        unregister_chrdev_region(dev_number, 1);
        return -1;
    }

    pr_info("wq_demo: device created at /dev/%s\n", DEVICE_NAME);

    return 0;
}

static void __exit wq_driver_exit(void)
{
    device_destroy(wq_class, dev_number);
    class_destroy(wq_class);
    cdev_del(&wq_cdev);
    unregister_chrdev_region(dev_number, 1);

    pr_info("wq_demo: module unloaded\n");
}

module_init(wq_driver_init);
module_exit(wq_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Neel");
MODULE_DESCRIPTION("Basic wait queue character driver demo");

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "ioctl_demo"
#define CLASS_NAME  "ioctl_demo_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Character device with ioctl examples");

/* Ioctl definitions */
#define IOCTL_MAGIC 'k'
#define IOCTL_GET_MSG   _IOR(IOCTL_MAGIC, 1, char *)
#define IOCTL_SET_MSG   _IOW(IOCTL_MAGIC, 2, char *)
#define IOCTL_GET_NUM   _IOR(IOCTL_MAGIC, 3, int *)
#define IOCTL_SET_NUM   _IOW(IOCTL_MAGIC, 4, int *)
#define IOCTL_EXCHANGE  _IOWR(IOCTL_MAGIC,5, struct ioctl_data *)

struct ioctl_data {
    int num;
    char msg[128];
};

static dev_t dev_number;
static struct cdev demo_cdev;
static struct class *demo_class;
static struct device *demo_device;

static char device_buffer[128] = "Default kernel message";
static int device_num = 0;

static int demo_open(struct inode *inode, struct file *file)
{
    pr_info("%s: device opened\n", DEVICE_NAME);
    return 0;
}

static int demo_release(struct inode *inode, struct file *file)
{
    pr_info("%s: device released\n", DEVICE_NAME);
    return 0;
}

static ssize_t demo_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    size_t len = strnlen(device_buffer, sizeof(device_buffer));
    if (*ppos >= len)
        return 0;
    if (count > len - *ppos)
        count = len - *ppos;
    if (copy_to_user(buf, device_buffer + *ppos, count))
        return -EFAULT;
    *ppos += count;
    return count;
}

static ssize_t demo_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if (count >= sizeof(device_buffer))
        count = sizeof(device_buffer) - 1;
    if (copy_from_user(device_buffer, buf, count))
        return -EFAULT;
    device_buffer[count] = '\0';
    return count;
}

static long demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int temp_num;

    switch (cmd) {
    case IOCTL_SET_MSG:
        if (copy_from_user(device_buffer, (char __user *)arg, sizeof(device_buffer)))
            return -EFAULT;
        device_buffer[sizeof(device_buffer)-1] = '\0';
        pr_info("%s: IOCTL_SET_MSG -> %s\n", DEVICE_NAME, device_buffer);
        break;

    case IOCTL_GET_MSG:
        if (copy_to_user((char __user *)arg, device_buffer, sizeof(device_buffer)))
            return -EFAULT;
        pr_info("%s: IOCTL_GET_MSG\n", DEVICE_NAME);
        break;

    case IOCTL_SET_NUM:
        if (copy_from_user(&temp_num, (int __user *)arg, sizeof(int)))
            return -EFAULT;
        device_num = temp_num;
        pr_info("%s: IOCTL_SET_NUM -> %d\n", DEVICE_NAME, device_num);
        break;

    case IOCTL_GET_NUM:
        temp_num = device_num;
        if (copy_to_user((int __user *)arg, &temp_num, sizeof(int)))
            return -EFAULT;
        pr_info("%s: IOCTL_GET_NUM -> %d\n", DEVICE_NAME, device_num);
        break;

    case IOCTL_EXCHANGE: {
        struct ioctl_data user_data, old;

        if (copy_from_user(&user_data, (struct ioctl_data __user *)arg, sizeof(user_data)))
            return -EFAULT;

        /* save old kernel values */
        old.num = device_num;
        strncpy(old.msg, device_buffer, sizeof(old.msg));
        old.msg[sizeof(old.msg)-1] = '\0';

        /* update kernel with user values */
        device_num = user_data.num;
        strncpy(device_buffer, user_data.msg, sizeof(device_buffer));
        device_buffer[sizeof(device_buffer)-1] = '\0';

        /* copy old back to user */
        if (copy_to_user((struct ioctl_data __user *)arg, &old, sizeof(old)))
            return -EFAULT;

        pr_info("%s: IOCTL_EXCHANGE swapped num %d and msg %s\n", DEVICE_NAME, device_num, device_buffer);
        break;
    }

    default:
        pr_info("%s: Unknown IOCTL cmd: %u\n", DEVICE_NAME, cmd);
        return -ENOTTY;
    }

    return 0;
}

static const struct file_operations demo_fops = {
    .owner = THIS_MODULE,
    .open = demo_open,
    .release = demo_release,
    .read = demo_read,
    .write = demo_write,
    .unlocked_ioctl = demo_ioctl,
};

static int __init demo_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate char dev region\n");
        return ret;
    }

    cdev_init(&demo_cdev, &demo_fops);
    demo_cdev.owner = THIS_MODULE;

    ret = cdev_add(&demo_cdev, dev_number, 1);
    if (ret) {
        pr_err("Failed to add cdev\n");
        unregister_chrdev_region(dev_number,1);
        return ret;
    }

    demo_class = class_create(CLASS_NAME);
    if (IS_ERR(demo_class)) {
        pr_err("Failed to create class\n");
        cdev_del(&demo_cdev);
        unregister_chrdev_region(dev_number,1);
        return PTR_ERR(demo_class);
    }

    demo_device = device_create(demo_class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(demo_device)) {
        pr_err("Failed to create device\n");
        class_destroy(demo_class);
        cdev_del(&demo_cdev);
        unregister_chrdev_region(dev_number,1);
        return PTR_ERR(demo_device);
    }

    pr_info("%s: initialized with major %d minor %d\n", DEVICE_NAME, MAJOR(dev_number), MINOR(dev_number));
    return 0;
}

static void __exit demo_exit(void)
{
    device_destroy(demo_class, dev_number);
    class_destroy(demo_class);
    cdev_del(&demo_cdev);
    unregister_chrdev_region(dev_number,1);
    pr_info("%s: exited\n", DEVICE_NAME);
}

module_init(demo_init);
module_exit(demo_exit);

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/slab.h>

#define PROC_NAME "my_proc_device"
#define BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Proc File System LDD Module");

static char proc_buffer[BUFFER_SIZE] = "Hello from Proc Device!\n";
static int current_position = 0;

/* Open function */
static int my_open(struct inode *inode, struct file *file)
{
    pr_info("Proc device opened\n");
    current_position = 0;
    return 0;
}

/* Read function */
static ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int len = strlen(proc_buffer);
    // int ret = 0;

    pr_info("Proc device read called. Position: %lld, Count: %zu\n", *ppos, count);

    /* Check if we are at the end of the file */
    if (*ppos >= len)
        return 0;

    /* Adjust count if necessary */
    if (count > len - *ppos)
        count = len - *ppos;

    /* Copy data to user space */
    if (copy_to_user(buf, proc_buffer + *ppos, count))
        return -EFAULT;

    /* Update position */
    *ppos += count;
    
    pr_info("Successfully read %zu bytes\n", count);
    return count;
}

/* Write function */
static ssize_t my_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    pr_info("Proc device write called. Count: %zu\n", count);

    /* Limit write size */
    if (count > BUFFER_SIZE - 1)
        count = BUFFER_SIZE - 1;

    /* Copy data from user space */
    if (copy_from_user(proc_buffer, buf, count))
        return -EFAULT;

    /* Add null terminator */
    proc_buffer[count] = '\0';
    
    pr_info("Successfully wrote %zu bytes\n", count);
    pr_info("Buffer content: %s\n", proc_buffer);

    return count;
}

/* Lseek function */
static loff_t my_lseek(struct file *file, loff_t offset, int whence)
{
    loff_t new_pos = 0;
    int len = strlen(proc_buffer);

    pr_info("Proc device lseek called. Offset: %lld, Whence: %d\n", offset, whence);

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file->f_pos + offset;
            break;
        case SEEK_END:
            new_pos = len + offset;
            break;
        default:
            return -EINVAL;
    }

    /* Boundary checks */
    if (new_pos < 0)
        return -EINVAL;

    file->f_pos = new_pos;
    pr_info("New position: %lld\n", new_pos);

    return new_pos;
}

/* Release function */
static int my_release(struct inode *inode, struct file *file)
{
    pr_info("Proc device released. Final position: %lld\n", file->f_pos);
    return 0;
}

/* File operations structure */
static const struct proc_ops proc_fops = {
    .proc_open    = my_open,
    .proc_read    = my_read,
    .proc_write   = my_write,
    .proc_lseek   = my_lseek,
    .proc_release = my_release,
};

static struct proc_dir_entry *proc_entry;

/* Module initialization */
static int __init proc_module_init(void)
{
    pr_info("Initializing Proc Module...\n");

    /* Create proc entry */
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &proc_fops);

    if (proc_entry == NULL) {
        pr_err("Failed to create proc entry\n");
        return -ENOMEM;
    }

    pr_info("Proc entry created successfully: /proc/%s\n", PROC_NAME);
    return 0;
}

/* Module cleanup */
static void __exit proc_module_exit(void)
{
    if (proc_entry) {
        proc_remove(proc_entry);
        pr_info("Proc entry removed\n");
    }
    pr_info("Exiting Proc Module\n");
}

module_init(proc_module_init);
module_exit(proc_module_exit);

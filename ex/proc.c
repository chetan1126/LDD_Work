// proc_driver.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROC_NAME "myproc"
#define BUF_SIZE 100

static char kbuf[BUF_SIZE];

static int my_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "PROC: Open Called\n");
    return 0;
}

static ssize_t my_read(struct file *file,char __user *ubuf,size_t count,loff_t *ppos)
{
    int len;

    len = strlen(kbuf);

    return simple_read_from_buffer(ubuf,count,ppos,kbuf,len);
}

static ssize_t my_write(struct file *file,const char __user *ubuf,size_t count,loff_t *ppos)
{
    if(count > BUF_SIZE-1)
        count = BUF_SIZE-1;

    if(copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    printk(KERN_INFO "PROC: Data Written = %s\n", kbuf);

    return count;
}

static loff_t my_lseek(struct file *file,loff_t offset,int whence)
{
    printk(KERN_INFO "PROC: Lseek Called\n");

    return default_llseek(file, offset, whence);
}

static int my_release(struct inode *inode,
                      struct file *file)
{
    printk(KERN_INFO "PROC: Release Called\n");
    return 0;
}

static const struct proc_ops proc_fops =
{
    .proc_open    = my_open,
    .proc_read    = my_read,
    .proc_write   = my_write,
    .proc_lseek   = my_lseek,
    .proc_release = my_release,
};

static int __init proc_init(void)
{
    proc_create(PROC_NAME,0666,NULL,&proc_fops);

    printk(KERN_INFO "PROC Entry Created\n");

    return 0;
}

static void __exit proc_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);

    printk(KERN_INFO "PROC Entry Removed\n");
}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chetan");
MODULE_DESCRIPTION("Proc File System Driver");
// 
#include <linux/init.h>  	//__init and __exit
#include <linux/module.h>	// module_init() module_exit() mandatory header
#include <linux/kernel.h>	// pr_info()
#include <linux/kthread.h>	//kernel thread API kthread_create()
#include <linux/mutex.h>	// provide mutex API
#include <linux/completion.h> 	//for completion mechanism and thread synchroization
#include <linux/delay.h>	//provide delay APIs like udelay and msleep
#include <linux/smp.h>		//provide multicore helper  API
#include <linux/moduleparam.h> 	//Allows passing parameter when inserting modules
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>


struct mutex buffer_lock;
mutex_init(&buffer_lock);


static char kernel_buffer[buffer_size];
static struct task_struct *thread1;	//each thread in kernel is represented by task_struct
static struct task_struct *thread2;	// used by kthread_create()


static DECLARE_COMPLETION(done_thread1);	// tells module init that thread1 has finished
static DECLARE_COMPLETION(done_thread2);


static const struct file_operations fops =
{	
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release,
	.read = my_read,
	.write = my_write
};

static int worker_function(void* arg)
{
	//This function is implemented by both threads.
	long id = (long) arg;	//thread eceives an argument
	unsigned int i;
	unsigned long temp;
	
	pr_info("mutex: thread %ld waiting ON CPU %d\n", id, smp_processor_id());
	wait_for_completion(&start_signal);
	
	pr_info("mutex: thread %ld started on CPU %d\n",id, smp_processor_id());
	
	mutex_lock(&buffer_lock);
	
	
	if(copy_from_user(kernel_buffer, user_buffer, 5) != 0)
	{
		pr_err("failed to copy data from user\n");
		return -EFAULT;
	}
	
	
	mutex_unlock(&buffer_lock);
	
	
	
	pr_info("mutex: Threads %ld finished on CPU %d\n",id , smp_processor_id());
	if(id == 1)
		complete(&done_thread1);
	else
		complete(&done_thread2);
	return 0;
}




static ssize_t my_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
	int bytes_to_read;
	
	pr_info("Read function called\n");
	
	if(*offset >= strlen(kernel_buffer))
	{
		pr_info("End of file reached\n");
		return 0;
	
	}
	
	bytes_to_read = min(count, (strlen(kernel_buffer) - (size_t) *offset));
	
	if(copy_to_user(user_buffer, kernel_buffer+*offset, bytes_to_read) != 0)
	{
		pr_err("failed to copy data to user\n");
		return -EFAULT;
	}
	
	*offset = *offset + bytes_to_read;
	pr_info("Sent %d bytes to the user\n", bytes_to_read);
	return bytes_to_read;
	
}
static ssize_t my_write(struct file *file, const char __user *user_buffer,size_t count, loff_t *offset)
{
	int bytes_to_write;
	int ret;
	pr_info("Write function called\n");
	if(*offset > buffer_size)
	{
		pr_info("End of file space\n");
		return -ENOSPC;
	}
	
	bytes_to_write = min(count, (size_t)((buffer_size-1) - *offset));
	
	 
	ret = copy_from_user(kernel_buffer+*offset, user_buffer, bytes_to_write);
	
	if(ret != 0)
	{
		pr_err("failed to copy data from user\n");
		return -EFAULT;
	}
	
	*offset += bytes_to_write;
	
	kernel_buffer[*offset] = '\0';
	pr_info("recived %d bytes from the user\n", bytes_to_write);
	pr_info("recived string = %s\n", kernel_buffer);
	return bytes_to_write;
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


static int __init my_init(void)
{
	printk(KERN_INFO"HELLO KERNEL\n");
	return 0;
	
}

static void __exit my_exit(void)
{
	printk(KERN_INFO"GOODBYE FROM MUTEX\n");

}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("Assignment 1 Mutex");
MODULE_VERSION("1.0");

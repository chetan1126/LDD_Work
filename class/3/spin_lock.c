#include <linux/init.h>  	//__init and __exit
#include <linux/module.h>	// module_init() module_exit() mandatory header
#include <linux/kernel.h>	// pr_info()
#include <linux/kthread.h>	//kernel thread API kthread_create()
#include <linux/spinlock.h>	// provide spinlock API
#include <linux/completion.h> 	//for completion mechanism and thread synchroization
#include <linux/delay.h>	//provide delay APIs like udelay and msleep
#include <linux/smp.h>		//provide multicore helper  API
#include <linux/modulepara.h> 	//Allows passing parameter when inserting modules

static unsigned long shared_counter;	//share variable

static spinlock_t counter_lock;	//spinlock variable to save ahared variable

static struct task_struct *thread1;	//each thread in kernel is represented by task_struct
static struct task_struct *thread2;	// used by kthread_create()

static DECLARE_COMPLETION(start_signal);	//completion variable used for synchronized execution
						//start signal makes both thread wait until module
						//init allow themto start

static DECLARE_COMPLETION(done_thread1);	// tells module init that thread1 has finished
static DECLARE_COMPLETION(done_thread2);

static unsigned int loops = 1000000;		// this variabletell how many times each thread
						//increment dhared counter
module_param(loops, unit,0444);	// this allow loops to pass through command line
					//insmod 0444 means parameter is readable from sysfs
					//but not writable after module loading
				
MODULE_PARM_DESC(loops, "Number of increments per threads");

static int use_lock =1;	//1= use spin lock, 0= don't use spin lock
module_param(use_lock, int, 0444);
MODULE_PARM_DESC(use_lock, "Use spinlocks 1=yes, 0 = no");

static int __init spin_demo_init(void)
{
	
}

static void __exit spin_demo_exit(void)
{
	pr_info("spin demo: Module unloaded\n");
	
}

module_init(spin_demo_init);
module_exit(spin_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("BEGINEERS SPINLOCK CODE");
MODULE_VERSION("1.0");

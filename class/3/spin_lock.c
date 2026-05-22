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
	unsigned long expected; 	// stores expected value of shared counter
	int cpu0,cpu1;			// variable number for cpu cores
	
	pr_info("spin_demo: modeule loaded\n");
	shared_couner = 0;
	spin_lock_init(&counter_lock);
	
	pr_info("spin_demo: Use_lock = %d \n",use_lock);
	pr_info("spin_demo: loops per thread = %u\n",loops);
	pr_info("spin-demo: onlinecpus = %u\n", num_online_cpu()); 	//Number of CPUs available
	
	//Creates kernel thread, worker function, argument passed to worker function
	// and name of kernel thread.
	thread1 = kthread_create(worker_function,(void*)1, "spin_threads1");
	if(IS_ERR(thread1))
	{
		pr_err("spin_demo: failed to create thread 1\n");
		return PTR_ERR(thread1);
	}
	
	thread2 = kthread_create(worker_function,(void*)2, "spin_threads1");
	if(IS_ERR(thread2))
	{
		pr_err("spin_demo: failed to create thread 2\n");
		return PTR_ERR(thread2);
	}
	 cpu0 = cpumask_first(cpu_onlinne_mask);	//cpu_online_mask contains all online CPUs
	 						// return unsigned long
	 cpu1 = cpumask_next(cpu0, cpu_online_mask);
	 pr_info("CPU online mask = %d", cpu_online_mask->bits[0]);		// 
	 if(cpu1 < nr_cpu_ids)
	 {
	 	kthread_bind(thread1, cpu0);
	 	kthrad_bind(thread2, cpu1);
	 	
	 	pr_info("spin-demo: thread1 bound to CPU %d\n", cpu0);
	 	pr_info("spin_demo: thread2 bound to CPU %d\n", cpu1);
	 }
	 else
	 {
	 	pr_warn("spin_demo: only one cou ionline. Race may not be visible.\n");
	 	//Race around can still happen due to preemption.
	 }
	 
	 wake_up_process(thread1);
	 wake_up_process(thread2);	//kthread_creat() thtaed insleeping mode
	 				//this function makes then runnable. Now execution jumps to
	 				//wait_for completion(&start_signal);
	 				//sleep for 100 miliseconds
	 bsleep(100);
	 
	 pr_info("spin_demo: Starting both thread together\n");
	 
	 complete_all(&start_signal); 	// wake all thread waitinf on start signal. both threads
	 					//start incrementing now
	 
	 wait_for_completion(&done_thread1);
	 wait_for_completion(&done_thread2);
	 
	 expected = 2UL*loops;
	 pr_info("spin_demo: Expected counter = %lU\n",expected);
	 pr_info("spin_demo: Actual counter =%lu\n", shared_counter);
	 
	 if(shared_counter == expected)
	 	pr_info("sppin_demo: RESULT: correct output\n");
	 else
	 	pr_info("spin_demo: RESULT: race around condition detected\n");
	 
	 return 0;
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

// spinlock_basic_demo.c
/*
| Header                  | Purpose                                                  |
| ----------------------- | -------------------------------------------------------- |
| `<linux/init.h>`        | Provides `__init` and `__exit` macros                    |
| `<linux/module.h>`      | Required for kernel module programming                   |
| `<linux/kernel.h>`      | Provides kernel logging macros like `pr_info()`          |
| `<linux/kthread.h>`     | Provides kernel thread APIs like `kthread_create()`      |
| `<linux/spinlock.h>`    | Provides spinlock APIs                                   |
| `<linux/completion.h>`  | Provides completion mechanism for thread synchronization |
| `<linux/delay.h>`       | Provides delay APIs like `udelay()` and `msleep()`       |
| `<linux/smp.h>`         | Provides SMP/multi-core helper APIs                      |
| `<linux/moduleparam.h>` | Allows passing parameters while inserting module         |
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/moduleparam.h>

static unsigned long shared_counter;    //SHared variable
static spinlock_t counter_lock;		//spinlock lock variable to save shared variable

static struct task_struct *thread1;	//Each thread in kernel is represented by task_struct
					//Used by kthread_create()
static struct task_struct *thread2;

static DECLARE_COMPLETION(start_signal);	//Completion variables used to synchronize execution 
						//Start signal makes both thread wait until module init allow them to start

static DECLARE_COMPLETION(done_thread1);	//Tells  module init that thread1 has finished
static DECLARE_COMPLETION(done_thread2);	//Tells module init that thread2 has finished

static unsigned int loops = 1000000;		//This variable tells how many times each thread increments shared counter
module_param(loops, uint, 0444);		//This allows loops to pass through command line when using insmod
						//0444 means parameter is readable from sysfs but not writable after module loading
MODULE_PARM_DESC(loops, "Number of increments per thread");

static int use_lock = 1;			//1 = use spin lock, 0 = don't use spin locks
module_param(use_lock, int, 0444);		//This allows use_lock to pass through command line when using insmod
MODULE_PARM_DESC(use_lock, "Use spinlock: 1=yes, 0=no");

static int worker_function(void *arg)		//This is the function executed by both kernel threads, when you create a kernel thread using kthread_create
{
	long id = (long)arg;			//Thread receives an argument. For thread1 (void *)1 and for thread2 (void *)2
	unsigned int i;
	unsigned long temp;

	pr_info("spin_demo: Thread %ld waiting on CPU %d\n",
		id, smp_processor_id());	//id - which thread it is. Custom number send by us we can send kernel thread pid as well current->pid, 
						//smp_processor_id - returns CPU core number starting from 0

	/*
	 * Both threads wait here.
	 * They will start together when module init calls complete_all().
	 */
	wait_for_completion(&start_signal);	//Both threads stop here till spin_demo_init() complete_all() is executed. Without this thread1 may start earlier than thread2 starts. That will reduces 
						//chances of race around. This makes both threads start at same time.

	pr_info("spin_demo: Thread %ld started on CPU %d\n",
		id, smp_processor_id());

	for (i = 0; i < loops; i++) {    	//main increment loop

		if (use_lock) {
			/*
			 * Protected section.
			 * Only one CPU/thread can enter this part at a time.
			 */
			spin_lock(&counter_lock);	//This tries to acquire spinlock. If no other CPU hold the lock the current thread enters the critical section. If 
							//another CPU holds the lock, the CPU keeps spinning until the lock is released. 

			shared_counter++;		//Although it looks one line. Internally it behaves like read, increment, write. Its not atomic operation.
							//Spinlock ensures that only one thread perfoms this sequence at a time. 

			spin_unlock(&counter_lock);	//Release the lock. After this another CPU can enter the critical section
		} else {
			/*
			 * Unsafe section.
			 *
			 * This is intentionally written in three steps:
			 * 1. Read shared_counter
			 * 2. Increment local temp
			 * 3. Write back to shared_counter
			 *
			 * Two CPU cores may interleave these steps and lose updates.
			 */
			temp = READ_ONCE(shared_counter);	//Read this varaible from memory. Do not optimize or chache this read

			/*
			 * Small delay to make race condition easier to observe.
			 */
			if ((i % 1000) == 0)			//For every 1000 iterations thread delays for 1 microsecond delay
				udelay(1);

			temp++;

			WRITE_ONCE(shared_counter, temp);	//Similary write the data to memory. DO not optimize it
		}
	}

	pr_info("spin_demo: Thread %ld finished on CPU %d\n",
		id, smp_processor_id());			//It prints thread has completed its loop

	if (id == 1)
		complete(&done_thread1);
	else
		complete(&done_thread2);

	return 0;
}

static int __init spin_demo_init(void)
{
	unsigned long expected;				//Stores the expected value of shared counter
	int cpu0, cpu1;					//variable number for two cpu cores. They are used to bind two kernel threads to two cpu cores

	pr_info("spin_demo: Module loaded\n");	

	shared_counter = 0;				//before start counter is set to 0.
	spin_lock_init(&counter_lock);			//This initializes spinlocks before use of spin_lock() and spin_unlock()

	pr_info("spin_demo: use_lock = %d\n", use_lock);	//Spinlock enabled or disabled
	pr_info("spin_demo: loops per thread = %u\n", loops);	//Number of increments per thread
	pr_info("spin_demo: online CPUs = %u\n", num_online_cpus());//Number of online CPUs

	/*
	 * Create two kernel threads.
	 */
	thread1 = kthread_create(worker_function, (void *)1, "spin_thread1");		//Creates kernel thread. worker function, argument passed to worker function  and name of kernel thread
	if (IS_ERR(thread1)) {								//CHecks if thread creation failed
		pr_err("spin_demo: Failed to create thread1\n");
		return PTR_ERR(thread1);						//extracts the error code
	}

	thread2 = kthread_create(worker_function, (void *)2, "spin_thread2");
	if (IS_ERR(thread2)) {
		pr_err("spin_demo: Failed to create thread2\n");
		return PTR_ERR(thread2);
	}

	/*
	 * Try to bind threads to two different CPUs.
	 * This helps demonstrate multi-core race condition.
	 */
	cpu0 = cpumask_first(cpu_online_mask);						//cpu_omline_mask contains all online cpu. cpumask_first returns the first online CPU.
	cpu1 = cpumask_next(cpu0, cpu_online_mask);					//Find next online cpu

	if (cpu1 < nr_cpu_ids) {							//Online cpu can only be found if cpu1 is less than maximum number of cpus available.
		kthread_bind(thread1, cpu0);						//Binding threads to cpus. This ensures both cpu core now can run in parallel.
		kthread_bind(thread2, cpu1);

		pr_info("spin_demo: Thread1 bound to CPU %d\n", cpu0);
		pr_info("spin_demo: Thread2 bound to CPU %d\n", cpu1);
	} else {
		pr_warn("spin_demo: Only one CPU online. Race may not be visible.\n");	//Race around can still happen due to preemption but in tha case we should use MUTEX. 
	}

	/*
	 * Start both threads.
	 */
	wake_up_process(thread1);					//kthread_create() threads in sleeping state. This makes them runnable. Now execution jumps to wait_for_completion(&start_signal)
	wake_up_process(thread2);

	msleep(100);							//sleep for 100 millisecond

	pr_info("spin_demo: Starting both threads together\n");

	complete_all(&start_signal);					//wake all threads waiting on start signal. Both threads start incrementing now

	/*
	 * Wait until both threads finish.
	 */
	wait_for_completion(&done_thread1);
	wait_for_completion(&done_thread2);

	expected = 2UL * loops;

	pr_info("spin_demo: Expected counter = %lu\n", expected);
	pr_info("spin_demo: Actual counter   = %lu\n", shared_counter);

	if (shared_counter == expected)
		pr_info("spin_demo: RESULT: Correct output\n");
	else
		pr_info("spin_demo: RESULT: Race condition detected\n");

	return 0;
}

static void __exit spin_demo_exit(void)
{
	pr_info("spin_demo: Module unloaded\n");
}

module_init(spin_demo_init);
module_exit(spin_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Neel");
MODULE_DESCRIPTION("Beginner friendly spinlock demo");

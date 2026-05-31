#include <linux/module.h>        // Required for Linux kernel module support
#include <linux/init.h>          // Provides __init and __exit macros
#include <linux/fs.h>            // Provides character device APIs and file_operations
#include <linux/cdev.h>          // Provides struct cdev and cdev helper APIs
#include <linux/device.h>        // Provides class_create(), device_create(), device_destroy()
#include <linux/uaccess.h>       // Provides copy_from_user() and simple_read_from_buffer()
#include <linux/timer.h>         // Provides struct timer_list and kernel timer APIs
#include <linux/jiffies.h>       // Provides jiffies, HZ, msecs_to_jiffies(), jiffies_to_msecs()
#include <linux/atomic.h>        // Provides atomic_t and atomic operations
#include <linux/string.h>        // Provides strncmp(), sscanf()
#include <linux/minmax.h>        // Provides min() macro
#include <linux/compiler.h>      // Provides READ_ONCE() and WRITE_ONCE()

#define DRIVER_NAME "timer_lab"          // Driver name used in logs and device registration
#define DEVICE_NAME "timer_lab"          // Device node name: /dev/timer_lab
#define CLASS_NAME  "timer_lab_class"    // Device class name visible under /sys/class/
#define CMD_BUF     64                   // Maximum command size accepted from user space

#define TIMER_STOPPED   0                // Timer mode: stopped
#define TIMER_PERIODIC  1                // Timer mode: periodic timer
#define TIMER_ONESHOT   2                // Timer mode: one-shot timer

static dev_t dev_num;                    // Stores dynamically allocated major and minor number

static struct cdev timer_cdev;           // Character device object

static struct class *timer_class;        // Device class pointer for automatic /dev node creation

static struct device *timer_device;      // Device pointer returned by device_create()

static struct timer_list lab_timer;      // Low-resolution kernel timer object

static atomic_t tick_count = ATOMIC_INIT(0);  
// Counts how many times the timer callback has executed

static atomic_t timer_mode = ATOMIC_INIT(TIMER_STOPPED);  
// Stores current timer mode: stopped, periodic, or one-shot

static unsigned int period_ms = 1000;    
// Timer period in milliseconds; default value is 1000 ms

static unsigned long last_tick_jiffies;  
// Stores jiffies value when the timer callback last executed

static unsigned long next_expiry_jiffies; 
// Stores the next programmed expiry time in jiffies

static const char *mode_to_string(int mode)
// Converts timer mode integer into readable string
{
	switch (mode) {                              // Check current timer mode

	case TIMER_STOPPED:                         // If timer is stopped
		return "stopped";                       // Return stopped string

	case TIMER_PERIODIC:                        // If timer is running periodically
		return "periodic";                      // Return periodic string

	case TIMER_ONESHOT:                         // If timer is one-shot
		return "oneshot";                       // Return oneshot string

	default:                                    // If invalid mode value appears
		return "unknown";                       // Return unknown string
	}
}

static unsigned long make_expiry_from_ms(unsigned int ms)
// Converts milliseconds into an absolute future jiffies expiry value
{
	unsigned long delay_jiffies;                // Stores converted delay in jiffies

	delay_jiffies = msecs_to_jiffies(ms);       // Convert milliseconds to jiffies

	if (delay_jiffies == 0)                     // Safety check for very small values
		delay_jiffies = 1;                      // Force at least one jiffy delay

	return jiffies + delay_jiffies;             // Return absolute expiry time
}

static void timer_callback(struct timer_list *t)
// This function is called when the low-resolution timer expires
{
	int count;                                  // Stores updated tick count

	int mode;                                   // Stores current timer mode

	unsigned int current_period;                // Stores current period in milliseconds

	unsigned long next_expiry;                  // Stores next expiry time in jiffies

	count = atomic_inc_return(&tick_count);     // Atomically increment timer tick count

	mode = atomic_read(&timer_mode);            // Read current timer mode

	current_period = READ_ONCE(period_ms);      // Safely read current period value

	WRITE_ONCE(last_tick_jiffies, jiffies);     // Store current jiffies as last callback time

	pr_info("%s: callback tick=%d mode=%s period=%u ms\n",
		DRIVER_NAME,
		count,
		mode_to_string(mode),
		current_period);
	// Print callback execution information into kernel log

	if (mode == TIMER_PERIODIC) {               // If timer is configured as periodic

		next_expiry = make_expiry_from_ms(current_period);
		// Calculate next expiry time using current jiffies + period

		WRITE_ONCE(next_expiry_jiffies, next_expiry);
		// Save next expiry value for status/debug read

		mod_timer(&lab_timer, next_expiry);
		// Rearm the timer; this makes one-shot timer_list behave periodically
	} else {                                    // If timer is not periodic

		atomic_set(&timer_mode, TIMER_STOPPED);
		// Mark timer as stopped after one-shot completion

		WRITE_ONCE(next_expiry_jiffies, 0UL);
		// Clear next expiry because timer is no longer pending
	}
}

static int my_open(struct inode *inode, struct file *file)
// Called when user opens /dev/timer_lab
{
	pr_info("%s: open\n", DRIVER_NAME);         // Print open message

	return 0;                                   // Return success
}

static int my_release(struct inode *inode, struct file *file)
// Called when user closes /dev/timer_lab
{
	pr_info("%s: release\n", DRIVER_NAME);      // Print close/release message

	return 0;                                   // Return success
}

static ssize_t my_read(struct file *file,
		       char __user *buf,
		       size_t len,
		       loff_t *off)
// Called when user reads from /dev/timer_lab
{
	char out[256];                              // Kernel buffer for formatted status output

	int n;                                      // Number of bytes written into out[]

	int mode;                                   // Stores current timer mode

	int pending;                                // Stores whether timer is currently pending

	unsigned long now;                          // Stores current jiffies

	unsigned long next;                         // Stores next expiry jiffies

	unsigned long last;                         // Stores last callback jiffies

	unsigned long remaining_ms = 0;             // Stores approximate remaining time in ms

	mode = atomic_read(&timer_mode);            // Read current timer mode

	pending = timer_pending(&lab_timer);        // Check whether timer is currently queued

	now = jiffies;                              // Read current global jiffies value

	next = READ_ONCE(next_expiry_jiffies);      // Read next programmed expiry time

	last = READ_ONCE(last_tick_jiffies);        // Read last callback execution time

	if (pending && time_after(next, now))        // Check if timer is pending and expiry is in future
		remaining_ms = jiffies_to_msecs(next - now);
		// Convert remaining jiffies into milliseconds

	n = scnprintf(out,
		      sizeof(out),
		      "mode=%s\n"
		      "period_ms=%u\n"
		      "tick_count=%d\n"
		      "timer_pending=%d\n"
		      "HZ=%d\n"
		      "now_jiffies=%lu\n"
		      "next_expiry_jiffies=%lu\n"
		      "remaining_ms=%lu\n"
		      "last_tick_jiffies=%lu\n",
		      mode_to_string(mode),
		      READ_ONCE(period_ms),
		      atomic_read(&tick_count),
		      pending,
		      HZ,
		      now,
		      next,
		      remaining_ms,
		      last);
	// Format timer status into a readable string

	return simple_read_from_buffer(buf, len, off, out, n);
	// Copy formatted kernel buffer to user space and handle file offset correctly
}

static ssize_t my_write(struct file *file,
			const char __user *buf,
			size_t len,
			loff_t *off)
// Called when user writes commands to /dev/timer_lab
{
	char kbuf[CMD_BUF];                         // Kernel buffer to store user command

	size_t n;                                   // Number of bytes copied from user

	unsigned int new_period;                    // Stores period value parsed from command

	unsigned long expiry;                       // Stores calculated expiry time in jiffies

	n = min(len, (size_t)(CMD_BUF - 1));        // Limit user input and reserve space for '\0'

	if (n == 0)                                 // If user wrote zero bytes
		return 0;                               // Nothing to process

	if (copy_from_user(kbuf, buf, n))           // Copy command from user space to kernel space
		return -EFAULT;                         // Return bad address error if copy fails

	kbuf[n] = '\0';                             // Null-terminate command string

	if (sscanf(kbuf, "start %u", &new_period) == 1) {
		// Command: start <period_ms>
		// Example: echo "start 1000" > /dev/timer_lab

		if (new_period == 0)                   // Period of 0 ms is invalid
			return -EINVAL;                     // Return invalid argument error

		WRITE_ONCE(period_ms, new_period);     // Store new timer period

		atomic_set(&timer_mode, TIMER_PERIODIC);
		// Set timer mode to periodic

		expiry = make_expiry_from_ms(new_period);
		// Calculate absolute expiry time in jiffies

		WRITE_ONCE(next_expiry_jiffies, expiry);
		// Store next expiry time for status read

		mod_timer(&lab_timer, expiry);
		// Start timer if inactive, or modify expiry if already active

		pr_info("%s: periodic timer started, period=%u ms\n",
			DRIVER_NAME,
			new_period);
		// Print start message
	}

	else if (sscanf(kbuf, "oneshot %u", &new_period) == 1) {
		// Command: oneshot <delay_ms>
		// Example: echo "oneshot 2000" > /dev/timer_lab

		if (new_period == 0)                   // Delay of 0 ms is invalid
			return -EINVAL;                     // Return invalid argument error

		WRITE_ONCE(period_ms, new_period);     // Store one-shot delay value

		atomic_set(&timer_mode, TIMER_ONESHOT);
		// Set timer mode to one-shot

		expiry = make_expiry_from_ms(new_period);
		// Calculate absolute expiry time in jiffies

		WRITE_ONCE(next_expiry_jiffies, expiry);
		// Store next expiry time

		mod_timer(&lab_timer, expiry);
		// Start one-shot timer

		pr_info("%s: one-shot timer started, delay=%u ms\n",
			DRIVER_NAME,
			new_period);
		// Print one-shot start message
	}

	else if (sscanf(kbuf, "period %u", &new_period) == 1) {
		// Command: period <period_ms>
		// Example: echo "period 500" > /dev/timer_lab

		if (new_period == 0)                   // Period of 0 ms is invalid
			return -EINVAL;                     // Return invalid argument error

		WRITE_ONCE(period_ms, new_period);     // Update period value

		if (timer_pending(&lab_timer)) {       // If timer is already active

			expiry = make_expiry_from_ms(new_period);
			// Calculate new expiry time

			WRITE_ONCE(next_expiry_jiffies, expiry);
			// Store new expiry for status read

			mod_timer(&lab_timer, expiry);
			// Modify active timer expiry
		}

		pr_info("%s: period changed to %u ms\n",
			DRIVER_NAME,
			new_period);
		// Print period change message
	}

	else if (!strncmp(kbuf, "stop", 4)) {
		// Command: stop
		// Example: echo "stop" > /dev/timer_lab

		atomic_set(&timer_mode, TIMER_STOPPED);
		// Stop future callback rearming

		timer_delete_sync(&lab_timer);
		// Deactivate the timer and wait if callback is currently running

		WRITE_ONCE(next_expiry_jiffies, 0UL);
		// Clear saved expiry value

		pr_info("%s: timer stopped\n", DRIVER_NAME);
		// Print stop message
	}

	else if (!strncmp(kbuf, "reset", 5)) {
		// Command: reset
		// Example: echo "reset" > /dev/timer_lab

		atomic_set(&tick_count, 0);
		// Reset callback execution counter

		WRITE_ONCE(last_tick_jiffies, 0UL);
		// Clear last callback timestamp

		pr_info("%s: tick counter reset\n", DRIVER_NAME);
		// Print reset message
	}

	else {
		// Unknown command path

		pr_info("%s: unknown command: %s\n", DRIVER_NAME, kbuf);
		// Print unknown command

		return -EINVAL;
		// Return invalid argument error
	}

	return len;                                  // Tell user space all bytes were consumed
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,                       // Prevent module unload while device is open
	.open = my_open,                            // Connect open() syscall to my_open()
	.release = my_release,                      // Connect close() syscall to my_release()
	.read = my_read,                            // Connect read() syscall to my_read()
	.write = my_write,                          // Connect write() syscall to my_write()
};

static int __init my_init(void)
// Module initialization function; executed during insmod
{
	int ret;                                    // Stores return values from kernel APIs

	ret = alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME);
	// Dynamically allocate one major/minor number pair

	if (ret < 0)                                // Check if device number allocation failed
		return ret;                             // Return error and fail module load

	cdev_init(&timer_cdev, &fops);
	// Initialize cdev and attach file_operations

	timer_cdev.owner = THIS_MODULE;
	// Set module owner for this character device

	ret = cdev_add(&timer_cdev, dev_num, 1);
	// Register cdev with VFS using allocated device number

	if (ret < 0) {                              // Check if cdev registration failed
		unregister_chrdev_region(dev_num, 1);   // Release allocated device number
		return ret;                             // Return error
	}

	timer_class = class_create(CLASS_NAME);
	// Create device class; latest kernels use class_create(name)

	if (IS_ERR(timer_class)) {                  // Check if class creation failed
		ret = PTR_ERR(timer_class);             // Convert error pointer to error code
		cdev_del(&timer_cdev);                  // Remove registered cdev
		unregister_chrdev_region(dev_num, 1);   // Release device number
		return ret;                             // Return error
	}

	timer_setup(&lab_timer, timer_callback, 0);
	// Initialize low-resolution timer and attach callback
	// This does not start the timer

	timer_device = device_create(timer_class,
				     NULL,
				     dev_num,
				     NULL,
				     DEVICE_NAME);
	// Create /dev/timer_lab automatically

	if (IS_ERR(timer_device)) {                 // Check if device node creation failed
		ret = PTR_ERR(timer_device);            // Convert error pointer to error code
		class_destroy(timer_class);             // Destroy class
		cdev_del(&timer_cdev);                  // Remove cdev
		unregister_chrdev_region(dev_num, 1);   // Release device number
		return ret;                             // Return error
	}

	pr_info("%s: loaded major=%d minor=%d HZ=%d\n",
		DRIVER_NAME,
		MAJOR(dev_num),
		MINOR(dev_num),
		HZ);
	// Print module load information

	pr_info("%s: commands: start <ms>, oneshot <ms>, period <ms>, stop, reset\n",
		DRIVER_NAME);
	// Print supported commands

	return 0;                                   // Return success
}

static void __exit my_exit(void)
// Module cleanup function; executed during rmmod
{
	atomic_set(&timer_mode, TIMER_STOPPED);
	// Mark timer as stopped

	timer_shutdown_sync(&lab_timer);
	// Final teardown API: stop timer, wait for callback, and prevent rearming

	device_destroy(timer_class, dev_num);
	// Remove /dev/timer_lab

	class_destroy(timer_class);
	// Destroy device class

	cdev_del(&timer_cdev);
	// Remove cdev from kernel

	unregister_chrdev_region(dev_num, 1);
	// Release major/minor number

	pr_info("%s: unloaded\n", DRIVER_NAME);
	// Print unload message
}

module_init(my_init);                         // Register module initialization function

module_exit(my_exit);                         // Register module cleanup function

MODULE_LICENSE("GPL");                       // Declare GPL license

MODULE_AUTHOR("Neel");                       // Declare module author

MODULE_DESCRIPTION("Low-resolution timer demo using latest Linux timer APIs");
// Describe module purpose

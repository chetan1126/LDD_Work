// high_rs_timer.c

#include <linux/module.h>       // Required for Linux kernel modules
#include <linux/init.h>         // Provides __init and __exit macros
#include <linux/kernel.h>       // Provides pr_info() and pr_err()
#include <linux/hrtimer.h>      // Provides high-resolution timer APIs
#include <linux/ktime.h>        // Provides ktime_t and time conversion helpers
#include <linux/atomic.h>       // Provides atomic_t and atomic operations
#include <linux/moduleparam.h>  // Provides module_param()
#include <linux/errno.h>        // Provides error codes such as -EINVAL

#define DRIVER_NAME "hrtimer_simple"   // Name used in kernel log messages

static struct hrtimer my_hrtimer;       // High-resolution timer object

static ktime_t interval;                // Stores timer interval in ktime_t format

static atomic_t tick_count = ATOMIC_INIT(0);
// Counts how many times the timer callback has executed

static unsigned int period_ms = 1000;
// Default timer period is 1000 milliseconds

module_param(period_ms, uint, 0444);
// Allows period_ms to be passed during insmod

MODULE_PARM_DESC(period_ms, "High-resolution timer period in milliseconds");
// Description shown by modinfo

static unsigned int max_ticks = 10;
// Default: stop timer after 10 ticks

module_param(max_ticks, uint, 0444);
// Allows max_ticks to be passed during insmod

MODULE_PARM_DESC(max_ticks, "Number of ticks before stopping; 0 means run continuously");
// Description shown by modinfo

static enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
// This function runs whenever the high-resolution timer expires
{
	int count;
	// Local variable to store updated tick count

	count = atomic_inc_return(&tick_count);
	// Atomically increment tick_count and return the new value

	pr_info("%s: high-resolution timer tick %d\n", DRIVER_NAME, count);
	// Print tick message in kernel log

	if (max_ticks != 0 && count >= max_ticks) {
		// If max_ticks is non-zero and required ticks are completed

		pr_info("%s: max_ticks reached, stopping timer\n", DRIVER_NAME);
		// Print stop message

		return HRTIMER_NORESTART;
		// Do not restart the hrtimer
	}

	hrtimer_forward_now(timer, interval);
	// Move expiry time forward by interval from current time
	// This is what makes the hrtimer periodic

	return HRTIMER_RESTART;
	// Tell kernel to restart the timer
}

static int __init hrtimer_demo_init(void)
// Module initialization function; runs during insmod
{
	if (period_ms == 0) {
		// A zero millisecond period is invalid

		pr_err("%s: period_ms cannot be zero\n", DRIVER_NAME);
		// Print error message

		return -EINVAL;
		// Fail module loading
	}

	atomic_set(&tick_count, 0);
	// Reset tick counter

	interval = ms_to_ktime(period_ms);
	// Convert milliseconds into ktime_t format

	hrtimer_setup(&my_hrtimer,
		      hrtimer_callback,
		      CLOCK_MONOTONIC,
		      HRTIMER_MODE_REL);
	// Initialize the hrtimer using latest kernel API
	// my_hrtimer        : timer object
	// hrtimer_callback  : function called on timer expiry
	// CLOCK_MONOTONIC   : clock source that does not go backward
	// HRTIMER_MODE_REL  : timer expiry is relative to current time

	hrtimer_start(&my_hrtimer, interval, HRTIMER_MODE_REL);
	// Start the hrtimer after interval duration

	pr_info("%s: module loaded\n", DRIVER_NAME);
	// Print module loaded message

	pr_info("%s: period_ms=%u max_ticks=%u\n",
		DRIVER_NAME, period_ms, max_ticks);
	// Print timer configuration

	return 0;
	// Return success
}

static void __exit hrtimer_demo_exit(void)
// Module cleanup function; runs during rmmod
{
	hrtimer_cancel(&my_hrtimer);
	// Cancel the timer and wait if callback is currently running

	pr_info("%s: module unloaded\n", DRIVER_NAME);
	// Print module unloaded message
}

module_init(hrtimer_demo_init);
// Register module initialization function

module_exit(hrtimer_demo_exit);
// Register module cleanup function

MODULE_LICENSE("GPL");
// Declare module license

MODULE_AUTHOR("Neel");
// Module author

MODULE_DESCRIPTION("Simple high-resolution timer example for latest kernel");
// Module description

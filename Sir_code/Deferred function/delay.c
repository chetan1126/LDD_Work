#include <linux/module.h>        // Provides core module support such as module_init(), module_exit(), MODULE_LICENSE()
#include <linux/init.h>          // Provides __init and __exit macros for init and exit functions
#include <linux/fs.h>            // Provides file_operations and character device interface definitions
#include <linux/cdev.h>          // Provides struct cdev and cdev-related APIs
#include <linux/uaccess.h>       // Provides copy_from_user(), copy_to_user(), and user-space access helpers
#include <linux/workqueue.h>     // Provides workqueue and delayed work APIs
#include <linux/delay.h>         // Provides udelay(), usleep_range(), and msleep()
#include <linux/ktime.h>         // Provides ktime_get_ns() for high-resolution time measurement
#include <linux/ktime.h>         // Duplicate include; harmless, but one copy is enough
#include <linux/mutex.h>         // Provides mutex APIs for protecting shared data
#include <linux/string.h>        // Provides string helper functions like strscpy()

#define DRIVER_NAME "time_work_lab"    // Name of the driver used in logs and device registration
#define CMD_BUF 64                     // Maximum size of command buffer received from user space

static dev_t dev_num;                  // Stores dynamically allocated major and minor device number
static struct cdev cdev_t;             // Character device structure used to register file operations

struct state                         // Structure used to store complete runtime state of the driver
{
    struct workqueue_struct *wq;       // Pointer to custom workqueue
    struct delayed_work dwork;         // Delayed work object executed after a scheduled delay
    struct mutex lock;                 // Mutex used to protect shared state variables

    u64 last_actual_ns;                // Stores measured actual delay in nanoseconds
    unsigned int last_requested;       // Stores delay value requested by user
    unsigned int last_work_delay_ms;   // Stores delayed work delay requested in milliseconds
    unsigned int work_runs;            // Counts how many times delayed work function executed
    bool work_pending;                 // Indicates whether delayed work is currently pending
    char last_cmd[32];                 // Stores last command executed by the driver
};

static struct state st;                // Global driver state variable

static void work_fn(struct work_struct *work)    // Function executed when delayed work runs
{
    struct state *s = container_of(               // Get pointer to parent struct state
        to_delayed_work(work),                    // Convert generic work_struct pointer to delayed_work pointer
        struct state,                             // Type of parent structure
        dwork                                     // Member name inside struct state
    );

    msleep(50);                                   // Sleep for 50 milliseconds inside workqueue context

    mutex_lock(&s->lock);                         // Lock mutex before modifying shared state

    s->work_runs++;                               // Increment delayed work execution counter
    s->work_pending = false;                      // Mark work as no longer pending
    strscpy(                                      // Safely copy string into last_cmd buffer
        s->last_cmd,                              // Destination buffer
        "delayed_work ran",                      // Source string
        sizeof(s->last_cmd)                       // Size of destination buffer
    );

    mutex_unlock(&s->lock);                       // Unlock mutex after updating shared state

    pr_info(                                      // Print informational message in kernel log
        "%s: delayed work executed, run = %u\n", // Log message format
        DRIVER_NAME,                              // Driver name
        s->work_runs                              // Number of work function executions
    );
}

static int my_open(struct inode *inode, struct file *file)    // Called when user opens the device file
{
    pr_info("%s: open \n", DRIVER_NAME);          // Print open message in kernel log

    return 0;                                     // Return success
}

static int my_release(struct inode *inode, struct file *file) // Called when user closes the device file
{
    pr_info("%s: close \n", DRIVER_NAME);         // Print close message in kernel log

    return 0;                                     // Return success
}

static ssize_t my_read(                           // Called when user reads from the device file
    struct file *file,                            // File pointer representing opened device file
    char __user *buf,                             // User-space buffer where data must be copied
    size_t len,                                   // Maximum number of bytes requested by user
    loff_t *off                                   // File offset used to support repeated reads properly
)
{
    char out[256];                                // Temporary kernel buffer to store formatted output
    int n;                                        // Stores number of characters written into output buffer

    u64 last_ns;                                  // Local copy of last actual delay
    unsigned int last_req;                        // Local copy of last requested delay
    unsigned int last_delay;                      // Local copy of last work delay
    unsigned int runs;                            // Local copy of work run counter
    bool pending;                                 // Local copy of work pending flag
    char last_cmd[32];                            // Local copy of last command string

    mutex_lock(&st.lock);                         // Lock mutex before reading shared state

    last_ns = st.last_actual_ns;                  // Copy measured delay from global state
    last_req = st.last_requested;                 // Copy requested delay from global state
    last_delay = st.last_work_delay_ms;           // Copy delayed work delay from global state
    runs = st.work_runs;                          // Copy work execution count from global state
    pending = st.work_pending;                    // Copy work pending status from global state
    strscpy(                                      // Safely copy last command string
        last_cmd,                                 // Destination local buffer
        st.last_cmd,                              // Source global command buffer
        sizeof(last_cmd)                          // Destination buffer size
    );

    mutex_unlock(&st.lock);                       // Unlock mutex after reading shared state

    n = scnprintf(                                // Format readable status information into output buffer
        out,                                      // Destination output buffer
        sizeof(out),                              // Maximum size of output buffer
        "last_cmd = %s requested = %u actual_ns = %llu, work_delay_ms = %u, work_runs = %u work_pending = %d\n",
                                                  // Output format string
        last_cmd,                                 // Last command executed
        last_req,                                 // Last requested delay value
        last_ns,                                  // Measured actual delay in nanoseconds
        last_delay,                               // Last delayed work delay in milliseconds
        runs,                                     // Number of delayed work executions
        pending                                   // Whether work is pending
    );

    return simple_read_from_buffer(               // Safely copy kernel buffer to user buffer
        buf,                                      // User-space destination buffer
        len,                                      // Number of bytes requested by user
        off,                                      // File offset
        out,                                      // Kernel source buffer
        n                                         // Number of valid bytes in source buffer
    );
}

static ssize_t my_write(                          // Called when user writes command to the device file
    struct file *file,                            // File pointer representing opened device file
    const char __user *buf,                       // User-space buffer containing command
    size_t len,                                   // Number of bytes written by user
    loff_t *off                                   // File offset, unused in this driver
)
{
    char kbuf[CMD_BUF];                           // Kernel buffer to store command copied from user space
    size_t n;                                     // Number of bytes to copy from user buffer
    unsigned int value;                           // Numeric value parsed from command
    u64 start_ns, end_ns;                         // Variables to measure start and end time in nanoseconds

    n = min(                                      // Choose smaller value between user length and buffer capacity
        len,                                      // Number of bytes user wants to write
        (size_t)(CMD_BUF - 1)                     // Maximum allowed bytes, leaving space for null terminator
    );

    if (copy_from_user(kbuf, buf, n))             // Copy command from user space to kernel space
        return -EFAULT;                           // Return bad address error if copy fails

    kbuf[n] = '\0';                               // Null terminate command string

    if (sscanf(kbuf, "udelay %u", &value) == 1)   // Check whether command is "udelay <value>"
    {
        if (value == 0 || value > 1000)           // Validate udelay value; avoid zero and very large busy waits
            return -EINVAL;                       // Return invalid argument error

        start_ns = ktime_get_ns();                // Capture start time in nanoseconds

        udelay(value);                            // Busy-wait delay for requested microseconds

        end_ns = ktime_get_ns();                  // Capture end time in nanoseconds

        mutex_lock(&st.lock);                     // Lock mutex before updating shared state

        st.last_requested = value;                // Store requested delay value
        st.last_actual_ns = end_ns - start_ns;    // Store measured actual delay duration
        strscpy(                                  // Store last command string safely
            st.last_cmd,                          // Destination command buffer
            "udelay",                             // Source command string
            sizeof(st.last_cmd)                   // Destination buffer size
        );

        mutex_unlock(&st.lock);                   // Unlock mutex after updating shared state

        pr_info(                                  // Print kernel log message
            "%s: udelay  %u us done\n",           // Log format
            DRIVER_NAME,                          // Driver name
            value                                 // Requested delay in microseconds
        );
    }

    else if (sscanf(kbuf, "usleep %u", &value) == 1) // Check whether command is "usleep <value>"
    {
        if (value == 0)                           // Validate that sleep value is not zero
            return -EINVAL;                       // Return invalid argument error

        start_ns = ktime_get_ns();                // Capture start time in nanoseconds

        usleep_range(value, value + 100);         // Sleep for a range between value and value + 100 microseconds

        end_ns = ktime_get_ns();                  // Capture end time in nanoseconds

        mutex_lock(&st.lock);                     // Lock mutex before updating shared state

        st.last_requested = value;                // Store requested sleep value
        st.last_actual_ns = end_ns - start_ns;    // Store measured actual sleep duration
        strscpy(                                  // Store last command string safely
            st.last_cmd,                          // Destination command buffer
            "usleep_range",                       // Source command string
            sizeof(st.last_cmd)                   // Destination buffer size
        );

        mutex_unlock(&st.lock);                   // Unlock mutex after updating shared state

        pr_info(                                  // Print kernel log message
            "%s: usleep_range %u us done\n",      // Log format
            DRIVER_NAME,                          // Driver name
            value                                 // Requested sleep value in microseconds
        );
    }

    else if (sscanf(kbuf, "msleep %u", &value) == 1) // Check whether command is "msleep <value>"
    {
        if (value == 0)                           // Validate that sleep value is not zero
            return -EINVAL;                       // Return invalid argument error

        start_ns = ktime_get_ns();                // Capture start time in nanoseconds

        msleep(value);                            // Sleep for requested milliseconds

        end_ns = ktime_get_ns();                  // Capture end time in nanoseconds

        mutex_lock(&st.lock);                     // Lock mutex before updating shared state

        st.last_requested = value;                // Store requested sleep value
        st.last_actual_ns = end_ns - start_ns;    // Store measured actual sleep duration
        strscpy(                                  // Store last command string safely
            st.last_cmd,                          // Destination command buffer
            "msleep",                             // Source command string
            sizeof(st.last_cmd)                   // Destination buffer size
        );

        mutex_unlock(&st.lock);                   // Unlock mutex after updating shared state

        pr_info(                                  // Print kernel log message
            "%s: msleep %u ms done\n",            // Log format
            DRIVER_NAME,                          // Driver name
            value                                 // Requested sleep value in milliseconds
        );
    }

    else if (sscanf(kbuf, "work %u", &value) == 1) // Check whether command is "work <value>"
    {
        mutex_lock(&st.lock);                     // Lock mutex before updating shared state

        st.last_work_delay_ms = value;            // Store requested delayed work delay in milliseconds
        st.work_pending = true;                   // Mark delayed work as pending
        strscpy(                                  // Store last command string safely
            st.last_cmd,                          // Destination command buffer
            "work queued",                        // Source command string
            sizeof(st.last_cmd)                   // Destination buffer size
        );

        mutex_unlock(&st.lock);                   // Unlock mutex after updating shared state

        queue_delayed_work(                       // Queue delayed work on custom workqueue
            st.wq,                                // Workqueue on which work should run
            &st.dwork,                            // Delayed work object
            msecs_to_jiffies(value)               // Convert delay from milliseconds to jiffies
        );

        pr_info(                                  // Print kernel log message
            "%s: delayed work queued after %u ms\n", // Log format
            DRIVER_NAME,                          // Driver name
            value                                 // Delay before work runs
        );
    }

    else if (!strncmp(kbuf, "cancel", 6))         // Check whether command starts with "cancel"
    {
        cancel_delayed_work_sync(&st.dwork);      // Cancel delayed work and wait if it is already running

        mutex_lock(&st.lock);                     // Lock mutex before updating shared state

        strscpy(                                  // Store last command string safely
            st.last_cmd,                          // Destination command buffer
            "work cancelled",                     // Source command string
            sizeof(st.last_cmd)                   // Destination buffer size
        );

        mutex_unlock(&st.lock);                   // Unlock mutex after updating shared state

        pr_info(                                  // Print kernel log message
            "%s: delayed work cancelled\n",       // Log format
            DRIVER_NAME                           // Driver name
        );
    }

    else                                          // If command does not match any supported format
    {
        return -EINVAL;                           // Return invalid argument error
    }

    return len;                                   // Return number of bytes accepted from user space
}

static struct file_operations fops =              // File operations table for the character device
{
    .open = my_open,                              // Connect open system call to my_open()
    .read = my_read,                              // Connect read system call to my_read()
    .write = my_write,                            // Connect write system call to my_write()
    .release = my_release,                        // Connect close system call to my_release()
};

static int __init my_init(void)                   // Module initialization function called during insmod
{
    int ret;                                      // Variable used to store return values from kernel APIs

    ret = alloc_chrdev_region(                    // Dynamically allocate device number
        &dev_num,                                 // Address where allocated device number will be stored
        0,                                        // First minor number
        1,                                        // Number of devices required
        DRIVER_NAME                               // Driver name shown in /proc/devices
    );

    if (ret < 0)                                  // Check whether device number allocation failed
    {
        return ret;                               // Return error code to stop module loading
    }

    cdev_init(                                    // Initialize character device structure
        &cdev_t,                                  // Address of cdev structure
        &fops                                     // File operations associated with this device
    );

    ret = cdev_add(                               // Add character device to kernel
        &cdev_t,                                  // Address of initialized cdev structure
        dev_num,                                  // Device number allocated earlier
        1                                         // Number of devices
    );

    if (ret < 0)                                  // Check whether cdev registration failed
    {
        unregister_chrdev_region(dev_num, 1);     // Free allocated device number
        return ret;                               // Return error code
    }

    mutex_init(&st.lock);                         // Initialize mutex used inside global state

    st.wq = alloc_workqueue(                      // Allocate custom workqueue
        DRIVER_NAME,                              // Name of workqueue
        0,                                        // Workqueue flags; zero means default behavior
        0                                         // Maximum active works; zero means default
    );

    if (!st.wq)                                   // Check whether workqueue allocation failed
    {
        cdev_del(&cdev_t);                        // Delete previously registered cdev
        unregister_chrdev_region(dev_num, 1);     // Free allocated device number
        return -ENOMEM;                           // Return out-of-memory error
    }

    INIT_DELAYED_WORK(                            // Initialize delayed work object
        &st.dwork,                                // Address of delayed work object
        work_fn                                   // Function to execute when delayed work runs
    );

    strscpy(                                      // Initialize last command string
        st.last_cmd,                              // Destination command buffer
        "none",                                  // Initial command value
        sizeof(st.last_cmd)                       // Destination buffer size
    );

    pr_info(                                      // Print module loaded message
        "%s loaded major %d minor %d\n",          // Log format
        DRIVER_NAME,                              // Driver name
        MAJOR(dev_num),                           // Extract major number from dev_t
        MINOR(dev_num)                            // Extract minor number from dev_t
    );

    return 0;                                     // Return success, module loaded successfully
}

static void __exit my_exit(void)                  // Module cleanup function called during rmmod
{
    cancel_delayed_work_sync(&st.dwork);          // Cancel pending delayed work and wait for running work to finish

    destroy_workqueue(st.wq);                     // Destroy custom workqueue

    cdev_del(&cdev_t);                            // Remove character device from kernel

    unregister_chrdev_region(dev_num, 1);         // Release allocated device number

    pr_info("%s unloaded \n", DRIVER_NAME);       // Print module unloaded message
}

module_init(my_init);                             // Register my_init() as module initialization function
module_exit(my_exit);                             // Register my_exit() as module cleanup function

MODULE_LICENSE("GPL");                           // Declare module license as GPL
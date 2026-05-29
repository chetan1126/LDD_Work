// basic_platform_char.c
// This file implements a basic platform driver.
// The platform driver's probe() function creates one character device.
// The character device supports open, release, read, and write operations.

#include <linux/module.h>          // Provides module_init(), module_exit(), MODULE_LICENSE(), THIS_MODULE.
#include <linux/kernel.h>          // Provides kernel logging APIs such as pr_info(), pr_err().
#include <linux/init.h>            // Provides __init and __exit macros.
#include <linux/fs.h>              // Provides file_operations, inode, file, alloc_chrdev_region().
#include <linux/cdev.h>            // Provides struct cdev, cdev_init(), cdev_add(), cdev_del().
#include <linux/device.h>          // Provides class_create(), device_create(), device_destroy().
#include <linux/uaccess.h>         // Provides copy_to_user() and copy_from_user().
#include <linux/slab.h>            // Provides devm_kzalloc() and other kernel memory allocation APIs.
#include <linux/mutex.h>           // Provides struct mutex, mutex_init(), mutex_lock(), mutex_unlock().
#include <linux/platform_device.h> // Provides platform_driver, platform_device, probe/remove framework.
#include <linux/version.h>         // Provides LINUX_VERSION_CODE and KERNEL_VERSION() macros.

// Name of the platform driver.
// The platform device name must match this name for probe() to be called.
#define DRIVER_NAME     "basic_pchar"

// Name of the character device node created under /dev.
// Final device file will be /dev/pchar0.
#define DEVICE_NAME     "pchar0"

// Name of the device class created under /sys/class/.
#define CLASS_NAME      "pchar_class"

// Size of the internal kernel buffer used by this character device.
#define BUFFER_SIZE     1024

/*
 * This structure represents the private data of our platform character device.
 *
 * Since this example creates only one device, only one object of this structure
 * is needed.
 *
 * In real embedded drivers, this structure usually stores:
 *      register base address
 *      IRQ number
 *      clock handles
 *      reset handles
 *      GPIO descriptors
 *      DMA channel information
 *      spinlocks or mutexes
 */
struct pchar_device {

	// Character device object used by the VFS character-device layer.
	struct cdev cdev;

	// Device number containing major and minor number.
	dev_t dev_num;

	// Kernel device object used for creating /dev/pchar0.
	struct device *device;

	// Pointer to the platform device that caused probe() to run.
	struct platform_device *pdev;

	// Internal kernel buffer used to store user-written data.
	char buffer[BUFFER_SIZE];

	// Number of valid bytes currently stored inside buffer.
	size_t data_size;

	// Mutex used to protect buffer and data_size from concurrent access.
	struct mutex lock;
};

/*
 * Global device number.
 *
 * alloc_chrdev_region() dynamically allocates a major number and minor number.
 * This variable stores that allocated device number.
 */
static dev_t pchar_dev_num;

/*
 * Device class pointer.
 *
 * This class is used by device_create() so that udev can create /dev/pchar0.
 */
static struct class *pchar_class;

/*
 * Software-created platform device.
 *
 * This example manually creates a platform device so the code can be tested
 * on a normal Linux machine or VM.
 *
 * In real embedded Linux, the platform device usually comes from Device Tree.
 */
static struct platform_device *pchar_platform_device;

/* ------------------------------------------------------------ */
/* open                                                         */
/* ------------------------------------------------------------ */

/*
 * pchar_open() is called when user-space opens /dev/pchar0.
 *
 * Example:
 *      int fd = open("/dev/pchar0", O_RDWR);
 *
 * Kernel flow:
 *      user open()
 *          -> system call
 *          -> VFS
 *          -> pchar_open()
 */
static int pchar_open(struct inode *inode, struct file *file)
{
	// Declare a pointer to our private device structure.
	struct pchar_device *pchar;

	/*
	 * inode->i_cdev points to the character device object that was opened.
	 *
	 * But we need the parent structure:
	 *      struct pchar_device
	 *
	 * container_of() converts the address of the cdev member into the
	 * address of the full parent structure.
	 */
	pchar = container_of(inode->i_cdev, struct pchar_device, cdev);

	/*
	 * Store the device pointer inside file->private_data.
	 *
	 * Later functions such as read(), write(), and release() receive
	 * struct file *file, so they can recover pchar using:
	 *
	 *      struct pchar_device *pchar = file->private_data;
	 */
	file->private_data = pchar;

	// Print device-specific log message.
	dev_info(&pchar->pdev->dev, "open called\n");

	// Return 0 to indicate open operation succeeded.
	return 0;
}

/* ------------------------------------------------------------ */
/* release                                                      */
/* ------------------------------------------------------------ */

/*
 * pchar_release() is called when user-space closes /dev/pchar0.
 *
 * Example:
 *      close(fd);
 *
 * This does not directly "close" the file.
 * The VFS closes the file descriptor.
 * This function only performs driver-specific cleanup.
 */
static int pchar_release(struct inode *inode, struct file *file)
{
	// Retrieve the private device pointer stored during open().
	struct pchar_device *pchar = file->private_data;

	// Print device-specific log message.
	dev_info(&pchar->pdev->dev, "release called\n");

	// Clear private_data for cleanliness.
	// This is optional because the file object is going away.
	file->private_data = NULL;

	// Return 0 to indicate release operation succeeded.
	return 0;
}

/* ------------------------------------------------------------ */
/* read                                                         */
/* ------------------------------------------------------------ */

/*
 * pchar_read() is called when user-space reads from /dev/pchar0.
 *
 * Example:
 *      read(fd, buffer, size);
 *
 * Or shell command:
 *      sudo cat /dev/pchar0
 */
static ssize_t pchar_read(struct file *file,
			  char __user *user_buffer,
			  size_t count,
			  loff_t *offset)
{
	// Retrieve the private device pointer stored during open().
	struct pchar_device *pchar = file->private_data;

	// Variable to store how many bytes are available to read.
	size_t available;

	// Variable to store how many bytes we will actually copy to user space.
	size_t bytes_to_read;

	// Return value of this read function.
	ssize_t ret;

	// Lock the mutex before accessing shared buffer and data_size.
	mutex_lock(&pchar->lock);

	/*
	 * If current file offset is greater than or equal to data_size,
	 * there is no more data to read.
	 *
	 * Returning 0 from read() means EOF.
	 * This is important because commands like cat stop when read() returns 0.
	 */
	if (*offset >= pchar->data_size) {

		// Set return value to 0 to indicate EOF.
		ret = 0;

		// Jump to common unlock-and-return section.
		goto out;
	}

	/*
	 * Calculate how many bytes are available from the current file offset.
	 *
	 * Example:
	 *      data_size = 20
	 *      offset    = 5
	 *      available = 15
	 */
	available = pchar->data_size - *offset;

	/*
	 * User may ask for more bytes than available.
	 *
	 * So read only:
	 *      minimum(user requested count, available data)
	 */
	bytes_to_read = min(count, available);

	/*
	 * Copy data from kernel buffer to user-space buffer.
	 *
	 * Source:
	 *      pchar->buffer + *offset
	 *
	 * Destination:
	 *      user_buffer
	 *
	 * copy_to_user() returns non-zero on failure.
	 */
	if (copy_to_user(user_buffer,
			 pchar->buffer + *offset,
			 bytes_to_read)) {

		// Return -EFAULT if user-space copy fails.
		ret = -EFAULT;

		// Jump to common unlock-and-return section.
		goto out;
	}

	/*
	 * Advance file offset by number of bytes successfully read.
	 *
	 * Example:
	 *      old offset = 0
	 *      bytes read = 5
	 *      new offset = 5
	 */
	*offset += bytes_to_read;

	// Return number of bytes successfully read.
	ret = bytes_to_read;

	// Print log message showing how many bytes were read.
	dev_info(&pchar->pdev->dev, "read %zu bytes\n", bytes_to_read);

// Common exit path for read function.
out:
	// Unlock mutex before returning.
	mutex_unlock(&pchar->lock);

	// Return result to VFS.
	return ret;
}

/* ------------------------------------------------------------ */
/* write                                                        */
/* ------------------------------------------------------------ */

/*
 * pchar_write() is called when user-space writes to /dev/pchar0.
 *
 * Example:
 *      write(fd, "hello", 5);
 *
 * Or shell command:
 *      echo "hello" | sudo tee /dev/pchar0
 */
static ssize_t pchar_write(struct file *file,
			   const char __user *user_buffer,
			   size_t count,
			   loff_t *offset)
{
	// Retrieve the private device pointer stored during open().
	struct pchar_device *pchar = file->private_data;

	// Variable to store actual number of bytes to write.
	size_t bytes_to_write;

	// Return value of this write function.
	ssize_t ret;

	// Lock the mutex before modifying shared buffer and data_size.
	mutex_lock(&pchar->lock);

	/*
	 * For simplicity, every write starts from beginning of buffer.
	 *
	 * This means every new write overwrites previous data.
	 *
	 * Example:
	 *      echo "ABC" > /dev/pchar0
	 *      cat /dev/pchar0
	 *
	 * Output:
	 *      ABC
	 */
	*offset = 0;

	/*
	 * Do not allow writing more than BUFFER_SIZE bytes.
	 *
	 * If user writes 2000 bytes and BUFFER_SIZE is 1024,
	 * only 1024 bytes will be copied.
	 */
	bytes_to_write = min(count, (size_t)BUFFER_SIZE);

	/*
	 * Clear old buffer content before writing new content.
	 *
	 * This prevents old leftover bytes from appearing after shorter writes.
	 */
	memset(pchar->buffer, 0, BUFFER_SIZE);

	/*
	 * Copy data from user-space buffer to kernel buffer.
	 *
	 * Destination:
	 *      pchar->buffer
	 *
	 * Source:
	 *      user_buffer
	 *
	 * copy_from_user() returns non-zero on failure.
	 */
	if (copy_from_user(pchar->buffer,
			   user_buffer,
			   bytes_to_write)) {

		// Return -EFAULT if user-space copy fails.
		ret = -EFAULT;

		// Jump to common unlock-and-return section.
		goto out;
	}

	/*
	 * Store number of valid bytes in the buffer.
	 *
	 * Only these bytes should be returned during read().
	 */
	pchar->data_size = bytes_to_write;

	/*
	 * Move file offset after the written data.
	 *
	 * Example:
	 *      wrote 10 bytes
	 *      offset becomes 10
	 */
	*offset = bytes_to_write;

	// Return number of bytes successfully written.
	ret = bytes_to_write;

	// Print log message showing how many bytes were written.
	dev_info(&pchar->pdev->dev, "wrote %zu bytes\n", bytes_to_write);

// Common exit path for write function.
out:
	// Unlock mutex before returning.
	mutex_unlock(&pchar->lock);

	// Return result to VFS.
	return ret;
}

/* ------------------------------------------------------------ */
/* file operations                                              */
/* ------------------------------------------------------------ */

/*
 * file_operations connects VFS operations to our driver functions.
 *
 * When user calls open(), VFS calls pchar_open().
 * When user calls read(), VFS calls pchar_read().
 * When user calls write(), VFS calls pchar_write().
 * When user calls close(), VFS eventually calls pchar_release().
 */
static const struct file_operations pchar_fops = {

	// Prevents module from being removed while file operations are active.
	.owner   = THIS_MODULE,

	// Connect open system call to pchar_open().
	.open    = pchar_open,

	// Connect close/release operation to pchar_release().
	.release = pchar_release,

	// Connect read system call to pchar_read().
	.read    = pchar_read,

	// Connect write system call to pchar_write().
	.write   = pchar_write,
};

/* ------------------------------------------------------------ */
/* platform probe                                               */
/* ------------------------------------------------------------ */

/*
 * pchar_probe() is called when the platform bus finds a matching device
 * for this driver.
 *
 * Matching happens because:
 *      platform device name = "basic_pchar"
 *      platform driver name = "basic_pchar"
 *
 * This is the most important function in a platform driver.
 * Character device creation happens here.
 */
static int pchar_probe(struct platform_device *pdev)
{
	// Pointer to private device structure.
	struct pchar_device *pchar;

	// Variable to store return values of kernel APIs.
	int ret;

	// Print log message when probe starts.
	dev_info(&pdev->dev, "probe called\n");

	/*
	 * Allocate memory for private device structure.
	 *
	 * devm_kzalloc() means:
	 *      devm  -> device managed
	 *      kzalloc -> allocate zero-filled memory
	 *
	 * Memory is automatically freed when the platform device is removed.
	 */
	pchar = devm_kzalloc(&pdev->dev, sizeof(*pchar), GFP_KERNEL);

	// Check whether memory allocation failed.
	if (!pchar)

		// Return out-of-memory error.
		return -ENOMEM;

	// Store platform device pointer inside private structure.
	pchar->pdev = pdev;

	// Store allocated character device number inside private structure.
	pchar->dev_num = pchar_dev_num;

	// Initially there is no data inside the buffer.
	pchar->data_size = 0;

	// Initialize mutex lock before using it.
	mutex_init(&pchar->lock);

	/*
	 * Initialize character device object.
	 *
	 * This connects cdev with file operations.
	 */
	cdev_init(&pchar->cdev, &pchar_fops);

	// Set owner of this character device to this module.
	pchar->cdev.owner = THIS_MODULE;

	/*
	 * Register character device with the VFS.
	 *
	 * After this, the kernel knows this device number is handled
	 * by pchar_fops.
	 */
	ret = cdev_add(&pchar->cdev, pchar->dev_num, 1);

	// Check if cdev_add() failed.
	if (ret < 0) {

		// Print error message.
		dev_err(&pdev->dev, "cdev_add failed\n");

		// Return error code.
		return ret;
	}

	/*
	 * Create device node /dev/pchar0.
	 *
	 * device_create() creates a device object and asks udev to create
	 * the corresponding /dev node.
	 */
	pchar->device = device_create(pchar_class,
				      &pdev->dev,
				      pchar->dev_num,
				      NULL,
				      DEVICE_NAME);

	// Check if device_create() failed.
	if (IS_ERR(pchar->device)) {

		// Convert error pointer into integer error code.
		ret = PTR_ERR(pchar->device);

		// Remove cdev because cdev_add() had already succeeded.
		cdev_del(&pchar->cdev);

		// Print error message.
		dev_err(&pdev->dev, "device_create failed\n");

		// Return error code.
		return ret;
	}

	/*
	 * Store private device pointer inside platform device.
	 *
	 * remove() can later retrieve this pointer using:
	 *      platform_get_drvdata(pdev)
	 */
	platform_set_drvdata(pdev, pchar);

	// Print log showing successful device creation.
	dev_info(&pdev->dev,
		 "created /dev/%s with major=%d minor=%d\n",
		 DEVICE_NAME,
		 MAJOR(pchar->dev_num),
		 MINOR(pchar->dev_num));

	// Return 0 to indicate successful probe.
	return 0;
}

/* ------------------------------------------------------------ */
/* platform remove                                              */
/* ------------------------------------------------------------ */

/*
 * pchar_remove() is called when the platform device is removed
 * or when the driver is unloaded.
 *
 * In this example, module exit unregisters the platform device.
 * That causes pchar_remove() to be called.
 */
static void pchar_remove(struct platform_device *pdev)
{
	// Retrieve private device structure stored during probe().
	struct pchar_device *pchar = platform_get_drvdata(pdev);

	// Print log message.
	dev_info(&pdev->dev, "remove called\n");

	// If private data is missing, nothing can be cleaned.
	if (!pchar)

		// Return directly.
		return;

	/*
	 * Destroy /dev/pchar0.
	 *
	 * This removes the user-visible device node.
	 */
	device_destroy(pchar_class, pchar->dev_num);

	/*
	 * Remove character device from VFS.
	 *
	 * After this, the device number is no longer connected to pchar_fops.
	 */
	cdev_del(&pchar->cdev);

	// Print log message.
	dev_info(&pdev->dev, "removed /dev/%s\n", DEVICE_NAME);
}

/* ------------------------------------------------------------ */
/* platform driver                                              */
/* ------------------------------------------------------------ */

/*
 * This structure registers our platform driver with the platform bus.
 */
static struct platform_driver pchar_platform_driver = {

	// probe() is called when a matching platform device is found.
	.probe  = pchar_probe,

	// remove() is called when the platform device is removed.
	.remove = pchar_remove,

	// Embedded device_driver structure.
	.driver = {

		// Driver name used for platform-device matching.
		.name  = DRIVER_NAME,

		// Owner of this driver is this kernel module.
		.owner = THIS_MODULE,
	},
};

/* ------------------------------------------------------------ */
/* module init                                                  */
/* ------------------------------------------------------------ */

/*
 * pchar_init() runs when module is inserted using insmod.
 *
 * Example:
 *      sudo insmod basic_platform_char.ko
 */
static int __init pchar_init(void)
{
	// Variable to store return values from kernel APIs.
	int ret;

	// Print module initialization message.
	pr_info("basic_pchar: module init\n");

	/*
	 * Dynamically allocate one character device number.
	 *
	 * pchar_dev_num will contain:
	 *      major number
	 *      minor number
	 */
	ret = alloc_chrdev_region(&pchar_dev_num, 0, 1, DEVICE_NAME);

	// Check whether device number allocation failed.
	if (ret < 0) {

		// Print error message.
		pr_err("basic_pchar: alloc_chrdev_region failed\n");

		// Return error code and stop module loading.
		return ret;
	}

	// Print allocated major and minor numbers.
	pr_info("basic_pchar: allocated major=%d minor=%d\n",
		MAJOR(pchar_dev_num),
		MINOR(pchar_dev_num));

	/*
	 * Create device class.
	 *
	 * This class appears under:
	 *      /sys/class/pchar_class
	 *
	 * Kernel API changed in Linux 6.4.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)

	// For Linux kernel 6.4 and newer.
	pchar_class = class_create(CLASS_NAME);

#else

	// For older Linux kernels.
	pchar_class = class_create(THIS_MODULE, CLASS_NAME);

#endif

	// Check whether class_create() failed.
	if (IS_ERR(pchar_class)) {

		// Convert error pointer into integer error code.
		ret = PTR_ERR(pchar_class);

		// Print error message.
		pr_err("basic_pchar: class_create failed\n");

		// Jump to cleanup path that releases allocated device number.
		goto unregister_chrdev;
	}

	/*
	 * Register platform driver with the platform bus.
	 *
	 * At this point, the driver is ready.
	 * But probe() will run only when a matching platform device exists.
	 */
	ret = platform_driver_register(&pchar_platform_driver);

	// Check whether platform driver registration failed.
	if (ret < 0) {

		// Print error message.
		pr_err("basic_pchar: platform_driver_register failed\n");

		// Jump to cleanup path that destroys class.
		goto destroy_class;
	}

	/*
	 * Create a software platform device.
	 *
	 * This is done only for learning/testing.
	 *
	 * Device name must match driver name:
	 *      DRIVER_NAME = "basic_pchar"
	 *
	 * Because names match, platform bus calls pchar_probe().
	 */
	pchar_platform_device =
		platform_device_register_simple(DRIVER_NAME,
						0,
						NULL,
						0);

	// Check whether platform device creation failed.
	if (IS_ERR(pchar_platform_device)) {

		// Convert error pointer into integer error code.
		ret = PTR_ERR(pchar_platform_device);

		// Print error message.
		pr_err("basic_pchar: platform_device_register_simple failed\n");

		// Jump to cleanup path that unregisters platform driver.
		goto unregister_platform_driver;
	}

	// Print successful platform-device registration message.
	pr_info("basic_pchar: platform device registered\n");

	// Return 0 to indicate module loaded successfully.
	return 0;

// Error cleanup path if platform device creation fails.
unregister_platform_driver:

	// Unregister platform driver.
	platform_driver_unregister(&pchar_platform_driver);

// Error cleanup path if platform driver registration fails.
destroy_class:

	// Destroy device class.
	class_destroy(pchar_class);

// Error cleanup path if class creation fails.
unregister_chrdev:

	// Release allocated device number.
	unregister_chrdev_region(pchar_dev_num, 1);

	// Return error code.
	return ret;
}

/* ------------------------------------------------------------ */
/* module exit                                                  */
/* ------------------------------------------------------------ */

/*
 * pchar_exit() runs when module is removed using rmmod.
 *
 * Example:
 *      sudo rmmod basic_platform_char
 */
static void __exit pchar_exit(void)
{
	// Print module exit message.
	pr_info("basic_pchar: module exit\n");

	/*
	 * Unregister software platform device.
	 *
	 * This causes the platform core to call:
	 *      pchar_remove()
	 */
	platform_device_unregister(pchar_platform_device);

	/*
	 * Unregister platform driver from platform bus.
	 */
	platform_driver_unregister(&pchar_platform_driver);

	/*
	 * Destroy device class.
	 */
	class_destroy(pchar_class);

	/*
	 * Release allocated major/minor number.
	 */
	unregister_chrdev_region(pchar_dev_num, 1);
}

// Register pchar_init() as module initialization function.
module_init(pchar_init);

// Register pchar_exit() as module cleanup function.
module_exit(pchar_exit);

// Declare module license.
MODULE_LICENSE("GPL");

// Declare module author.
MODULE_AUTHOR("Neel");

// Declare module description.
MODULE_DESCRIPTION("Basic platform driver with single character device");

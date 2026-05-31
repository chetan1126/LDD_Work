// multi_char_driver.c
// This is a Linux kernel module implementing multiple character devices.

// Include basic module macros such as module_init(), module_exit(), MODULE_LICENSE().
#include <linux/module.h>

// Include kernel logging macros such as pr_info() and pr_err().
#include <linux/kernel.h>

// Include __init and __exit macros for module initialization and cleanup functions.
#include <linux/init.h>

// Include file system structures such as file_operations, inode, file.
#include <linux/fs.h>

// Include character device structure and helper functions such as cdev_init() and cdev_add().
#include <linux/cdev.h>

// Include device model APIs such as class_create() and device_create().
#include <linux/device.h>

// Include user-kernel copy functions such as copy_to_user() and copy_from_user().
#include <linux/uaccess.h>

// Include kernel memory allocation functions such as kcalloc() and kfree().
#include <linux/slab.h>

// Include mutex APIs such as mutex_init(), mutex_lock(), and mutex_unlock().
#include <linux/mutex.h>

// Include kernel version macros such as LINUX_VERSION_CODE and KERNEL_VERSION().
#include <linux/version.h>

// Define the base name used while allocating character device numbers.
#define DEVICE_NAME     "multi_char"

// Define the device class name visible under /sys/class/.
#define CLASS_NAME      "multi_char_class"

// Define how many character devices this driver will create.
#define DEVICE_COUNT    4

// Define the size of each device's private kernel buffer.
#define BUFFER_SIZE     1024

// Define a structure representing one character device instance.
struct multi_char_dev {

	// Character device object registered with the VFS layer.
	struct cdev cdev;

	// Private kernel buffer for this particular device.
	char buffer[BUFFER_SIZE];

	// Number of valid bytes currently stored in this device buffer.
	size_t data_size;

	// Mutex lock to protect this device buffer from concurrent access.
	struct mutex lock;

	// Minor number of this device.
	int minor;
};

// Stores the first allocated device number.
// This contains both major and minor number.
static dev_t base_dev;

// Pointer to the device class used for automatic /dev node creation.
static struct class *multi_char_class;

// Pointer to dynamically allocated array of device structures.
static struct multi_char_dev *devices;

/* ---------- open ---------- */

// This function is called when user opens /dev/multi_charX.
static int multi_char_open(struct inode *inode, struct file *file)
{
	// Declare a pointer to our device-specific structure.
	struct multi_char_dev *dev;

	// Get the address of the parent structure from the cdev pointer.
	// inode->i_cdev points to the cdev belonging to the opened device.
	// container_of() gives the full struct multi_char_dev containing that cdev.
	dev = container_of(inode->i_cdev, struct multi_char_dev, cdev);

	// Store device pointer inside file->private_data.
	// Later read(), write(), release(), and llseek() can use this pointer.
	file->private_data = dev;

	// Print kernel log showing which minor device was opened.
	pr_info("multi_char: open called for minor %d\n", dev->minor);

	// Return 0 to indicate successful open.
	return 0;
}

/* ---------- release ---------- */

// This function is called when user closes /dev/multi_charX.
static int multi_char_release(struct inode *inode, struct file *file)
{
	// Get device pointer that was stored during open().
	struct multi_char_dev *dev = file->private_data;

	// Print kernel log showing which minor device was closed.
	pr_info("multi_char: release called for minor %d\n", dev->minor);

	// Return 0 to indicate successful close.
	return 0;
}

/* ---------- read ---------- */

// This function is called when user reads from /dev/multi_charX.
static ssize_t multi_char_read(struct file *file,
			       char __user *user_buffer,
			       size_t count,
			       loff_t *offset)
{
	// Get device pointer from file->private_data.
	// This tells us which minor device is being read.
	struct multi_char_dev *dev = file->private_data;

	// Stores number of bytes available from current offset.
	size_t available;

	// Stores actual number of bytes that will be copied to user space.
	size_t bytes_to_read;

	// Stores return value of this read function.
	ssize_t ret;

	// Lock the device before accessing its buffer and data_size.
	mutex_lock(&dev->lock);

	// If file offset is already at or beyond valid data size, there is nothing to read.
	if (*offset >= dev->data_size) {

		// Returning 0 from read() means End Of File.
		ret = 0;

		// Jump to common unlock and return section.
		goto out;
	}

	// Calculate how many valid bytes are available from the current file offset.
	available = dev->data_size - (size_t)(*offset);

	// Read only the minimum of requested bytes and available bytes.
	bytes_to_read = min(count, available);

	// Copy data from kernel buffer to user-space buffer.
	// dev->buffer + *offset gives the current read position inside kernel buffer.
	if (copy_to_user(user_buffer, dev->buffer + *offset, bytes_to_read)) {

		// Return -EFAULT if copying to user space fails.
		ret = -EFAULT;

		// Jump to common unlock and return section.
		goto out;
	}

	// Advance file offset by number of bytes successfully read.
	*offset += bytes_to_read;

	// Return number of bytes successfully read.
	ret = bytes_to_read;

	// Print kernel log showing read operation details.
	pr_info("multi_char: read %zu bytes from minor %d\n",
		bytes_to_read, dev->minor);

// Common exit label for read function.
out:
	// Unlock the device before returning.
	mutex_unlock(&dev->lock);

	// Return result to VFS.
	return ret;
}

/* ---------- write ---------- */

// This function is called when user writes to /dev/multi_charX.
static ssize_t multi_char_write(struct file *file,
				const char __user *user_buffer,
				size_t count,
				loff_t *offset)
{
	// Get device pointer from file->private_data.
	// This tells us which minor device is being written.
	struct multi_char_dev *dev = file->private_data;

	// Stores remaining space in the device buffer.
	size_t space_left;

	// Stores actual number of bytes that will be copied from user space.
	size_t bytes_to_write;

	// Stores return value of this write function.
	ssize_t ret;

	// Lock the device before modifying its buffer and data_size.
	mutex_lock(&dev->lock);

	// If file offset is already beyond buffer capacity, no space is available.
	if (*offset >= BUFFER_SIZE) {

		// Return -ENOSPC to indicate no space left on device.
		ret = -ENOSPC;

		// Jump to common unlock and return section.
		goto out;
	}

	// Calculate remaining writable space from current file offset.
	space_left = BUFFER_SIZE - (size_t)(*offset);

	// Write only the minimum of requested bytes and remaining buffer space.
	bytes_to_write = min(count, space_left);

	// Copy data from user-space buffer to kernel buffer.
	// dev->buffer + *offset gives the current write position inside kernel buffer.
	if (copy_from_user(dev->buffer + *offset, user_buffer, bytes_to_wriike)) {

		// Return -EFAULT if copying from user space fails.
		ret = -EFAULT;

		// Jump to common unlock and return section.
		goto out;
	}

	// Advance file offset by number of bytes successfully written.
	*offset += bytes_to_write;

	// If the new offset is beyond previous data_size, update data_size.
	if (dev->data_size < *offset)

		// Store new valid data size.
		dev->data_size = *offset;

	// Return number of bytes successfully written.
	ret = bytes_to_write;

	// Print kernel log showing write operation details.
	pr_info("multi_char: wrote %zu bytes to minor %d\n",
		bytes_to_write, dev->minor);

// Common exit label for write function.
out:
	// Unlock the device before returning.
	mutex_unlock(&dev->lock);

	// Return result to VFS.
	return ret;
}

/* ---------- llseek ---------- */

// This function is called when user changes file offset using lseek().
static loff_t multi_char_llseek(struct file *file, loff_t offset, int whence)
{
	// Get device pointer from file->private_data.
	struct multi_char_dev *dev = file->private_data;

	// Stores newly calculated file position.
	loff_t new_pos;

	// Lock the device while calculating position based on data_size.
	mutex_lock(&dev->lock);

	// Decide how to calculate new file position based on whence.
	switch (whence) {

	// SEEK_SET means offset is calculated from beginning of file.
	case SEEK_SET:

		// New position is directly equal to given offset.
		new_pos = offset;

		// Exit switch.
		break;

	// SEEK_CUR means offset is calculated from current file position.
	case SEEK_CUR:

		// New position is current file position plus given offset.
		new_pos = file->f_pos + offset;

		// Exit switch.
		break;

	// SEEK_END means offset is calculated from end of valid data.
	case SEEK_END:

		// New position is current data size plus given offset.
		new_pos = dev->data_size + offset;

		// Exit switch.
		break;

	// Invalid whence value.
	default:

		// Unlock before returning error.
		mutex_unlock(&dev->lock);

		// Return invalid argument error.
		return -EINVAL;
	}

	// Reject negative offset or offset beyond total buffer size.
	if (new_pos < 0 || new_pos > BUFFER_SIZE) {

		// Unlock before returning error.
		mutex_unlock(&dev->lock);

		// Return invalid argument error.
		return -EINVAL;
	}

	// Update file's current position.
	file->f_pos = new_pos;

	// Unlock the device after updating position.
	mutex_unlock(&dev->lock);

	// Return new file position.
	return new_pos;
}

/* ---------- file operations ---------- */

// This structure maps VFS operations to our driver functions.
static const struct file_operations multi_char_fops = {

	// Owner field prevents module from being removed while file operations are active.
	.owner   = THIS_MODULE,

	// Maps open system call to multi_char_open().
	.open    = multi_char_open,

	// Maps close system call to multi_char_release().
	.release = multi_char_release,

	// Maps read system call to multi_char_read().
	.read    = multi_char_read,

	// Maps write system call to multi_char_write().
	.write   = multi_char_write,

	// Maps lseek system call to multi_char_llseek().
	.llseek  = multi_char_llseek,
};

/* ---------- module init ---------- */

// This function runs when module is inserted using insmod.
static int __init multi_char_init(void)
{
	// Stores return values from kernel APIs.
	int ret;

	// Loop counter for creating multiple devices.
	int i;

	// Stores individual device number for each minor device.
	dev_t dev_num;

	// Print module initialization message.
	pr_info("multi_char: module init\n");

	// Dynamically allocate one major number and DEVICE_COUNT minor numbers.
	ret = alloc_chrdev_region(&base_dev, 0, DEVICE_COUNT, DEVICE_NAME);

	// Check whether device number allocation failed.
	if (ret < 0) {

		// Print error message if allocation failed.
		pr_err("multi_char: failed to allocate device numbers\n");

		// Return error code to stop module loading.
		return ret;
	}

	// Print allocated major number.
	pr_info("multi_char: major number = %d\n", MAJOR(base_dev));

	// Check if kernel version is 6.4.0 or newer.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)

	// For newer kernels, class_create() takes only class name.
	multi_char_class = class_create(CLASS_NAME);

	// For older kernels, class_create() takes module owner and class name.
#else

	// Create device class for older kernel versions.
	multi_char_class = class_create(THIS_MODULE, CLASS_NAME);

	// End kernel version conditional compilation.
#endif

	// Check whether class creation failed.
	if (IS_ERR(multi_char_class)) {

		// Print error message.
		pr_err("multi_char: failed to create class\n");

		// Convert error pointer into error number.
		ret = PTR_ERR(multi_char_class);

		// Jump to cleanup code that unregisters allocated device numbers.
		goto unregister_region;
	}

	// Allocate zero-initialized memory for DEVICE_COUNT device structures.
	devices = kcalloc(DEVICE_COUNT, sizeof(struct multi_char_dev), GFP_KERNEL);

	// Check whether memory allocation failed.
	if (!devices) {

		// Store out-of-memory error code.
		ret = -ENOMEM;

		// Jump to cleanup code that destroys class.
		goto destroy_class;
	}

	// Loop through all minor devices.
	for (i = 0; i < DEVICE_COUNT; i++) {

		// Create device number using allocated major and minor number i.
		dev_num = MKDEV(MAJOR(base_dev), MINOR(base_dev) + i);

		// Store minor number inside device structure.
		devices[i].minor = i;

		// Initially no valid data is present in the buffer.
		devices[i].data_size = 0;

		// Initialize mutex lock for this device.
		mutex_init(&devices[i].lock);

		// Initialize cdev structure and connect it with file operations.
		cdev_init(&devices[i].cdev, &multi_char_fops);

		// Set owner of cdev to this module.
		devices[i].cdev.owner = THIS_MODULE;

		// Add this character device to the kernel.
		ret = cdev_add(&devices[i].cdev, dev_num, 1);

		// Check whether cdev_add() failed.
		if (ret < 0) {

			// Print error message with failed minor number.
			pr_err("multi_char: cdev_add failed for minor %d\n", i);

			// Jump to cleanup previously created devices.
			goto cleanup_devices;
		}

		// Create /dev/multi_charX device node.
		if (IS_ERR(device_create(multi_char_class,
					 NULL,
					 dev_num,
					 NULL,
					 "multi_char%d",
					 i))) {

			// Print error if device node creation failed.
			pr_err("multi_char: device_create failed for minor %d\n", i);

			// Remove cdev because device_create failed after cdev_add succeeded.
			cdev_del(&devices[i].cdev);

			// Store generic invalid argument error.
			ret = -EINVAL;

			// Jump to cleanup previously created devices.
			goto cleanup_devices;
		}

		// Print success message for created device node.
		pr_info("multi_char: created /dev/multi_char%d\n", i);
	}

	// Return 0 if all devices were created successfully.
	return 0;

// Cleanup label used if device creation fails midway.
cleanup_devices:

	// Remove all devices that were successfully created before failure.
	while (--i >= 0) {

		// Recreate device number for cleanup.
		dev_num = MKDEV(MAJOR(base_dev), MINOR(base_dev) + i);

		// Destroy /dev/multi_charX device node.
		device_destroy(multi_char_class, dev_num);

		// Remove cdev from kernel.
		cdev_del(&devices[i].cdev);
	}

	// Free allocated device array.
	kfree(devices);

// Cleanup label used if device array allocation fails.
destroy_class:

	// Destroy device class.
	class_destroy(multi_char_class);

// Cleanup label used if class creation fails.
unregister_region:

	// Release allocated major and minor numbers.
	unregister_chrdev_region(base_dev, DEVICE_COUNT);

	// Return error code.
	return ret;
}

/* ---------- module exit ---------- */

// This function runs when module is removed using rmmod.
static void __exit multi_char_exit(void)
{
	// Loop counter for removing devices.
	int i;

	// Stores individual device number for each minor device.
	dev_t dev_num;

	// Print module exit message.
	pr_info("multi_char: module exit\n");

	// Loop through all created devices.
	for (i = 0; i < DEVICE_COUNT; i++) {

		// Create device number using allocated major and minor number i.
		dev_num = MKDEV(MAJOR(base_dev), MINOR(base_dev) + i);

		// Destroy /dev/multi_charX device node.
		device_destroy(multi_char_class, dev_num);

		// Remove cdev from kernel.
		cdev_del(&devices[i].cdev);

		// Print removal message.
		pr_info("multi_char: removed /dev/multi_char%d\n", i);
	}

	// Free dynamically allocated device array.
	kfree(devices);

	// Destroy device class.
	class_destroy(multi_char_class);

	// Release allocated major and minor numbers.
	unregister_chrdev_region(base_dev, DEVICE_COUNT);
}

// Register multi_char_init() as module insertion function.
module_init(multi_char_init);

// Register multi_char_exit() as module removal function.
module_exit(multi_char_exit);

// Declare module license as GPL.
MODULE_LICENSE("GPL");

// Declare module author.
MODULE_AUTHOR("Neel");

// Declare module description.
MODULE_DESCRIPTION("Multiple character devices using one driver and multiple minors");

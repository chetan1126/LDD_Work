# LDD Proc File System Module - Complete Guide

## Overview
This kernel module demonstrates a complete proc file system interface with file operations:
- `proc_open()` - Opens the proc device
- `proc_read()` - Reads data from the proc device
- `proc_write()` - Writes data to the proc device
- `proc_lseek()` - Seeks within the proc device
- `proc_release()` - Closes/releases the proc device

## Files
- `trial.c` - Main kernel module source code
- `Makefile` - Build configuration
- `run_module.sh` - Automated test script

## Prerequisites
You need to have the following installed:
```bash
sudo apt-get install build-essential linux-headers-$(uname -r)
```

## Quick Start (Automated)

```bash
cd /home/desd/chetan_kotrange/LDD_Work/ex/
chmod +x run_module.sh
sudo ./run_module.sh
```

## Manual Step-by-Step Instructions

### Step 1: Compile the Module
```bash
cd /home/desd/chetan_kotrange/LDD_Work/ex/
make clean          # Clean previous builds
make                # Compile the module
```

Expected output: `trial.ko` file created

### Step 2: Insert the Module into Kernel
```bash
sudo insmod trial.ko
```

### Step 3: Verify Module is Loaded
```bash
lsmod | grep trial
```

Or check kernel messages:
```bash
sudo dmesg | tail -5
```

### Step 4: The proc Device File
After inserting the module, the following file is created:
```
/proc/my_proc_device
```

Permissions: 0666 (read/write for all users)

---

## Testing Operations

### 1. READ Operation
Read the entire content:
```bash
sudo cat /proc/my_proc_device
```

Output: `Hello from Proc Device!`

Read specific number of bytes:
```bash
sudo dd if=/proc/my_proc_device bs=1 count=5 2>/dev/null
```

### 2. WRITE Operation
Write new data to the device:
```bash
sudo bash -c 'echo "Your custom data here" > /proc/my_proc_device'
```

Or using echo:
```bash
echo "New message" | sudo tee /proc/my_proc_device
```

Verify the write:
```bash
sudo cat /proc/my_proc_device
```

### 3. LSEEK Operation
Skip first 6 bytes and read next 15 bytes:
```bash
sudo bash -c 'dd if=/proc/my_proc_device bs=1 skip=6 count=15 2>/dev/null'
```

Seek from end (backward):
```bash
sudo bash -c 'dd if=/proc/my_proc_device bs=1 skip=100 2>/dev/null' | head -c 1
```

### 4. OPEN and RELEASE Operations
These are automatically called with any read/write operation:
```bash
exec 3< /proc/my_proc_device     # Open file descriptor
head -c 10 <&3                    # Read from it
exec 3<&-                         # Close/release
```

### 5. View All Operations in Kernel Log
```bash
sudo dmesg | grep "Proc device" | tail -20
```

---

## Code Explanation

### proc_open()
Called when the file is opened. Initializes position and logs the event.

### proc_read()
Reads data from the buffer and copies to user space. Handles:
- End-of-file checking
- Boundary adjustments
- Position tracking (*ppos)
- copy_to_user() for safe user space transfer

### proc_write()
Writes data from user space to kernel buffer. Handles:
- Size limitations (BUFFER_SIZE)
- copy_from_user() for safe kernel space transfer
- Null termination

### proc_lseek()
Seeks within the buffer. Supports:
- SEEK_SET - Absolute position
- SEEK_CUR - Relative to current position
- SEEK_END - Relative to end of buffer

### proc_release()
Called when the file is closed. Logs the final position.

---

## Advanced Testing

### Monitor Kernel Messages in Real-Time
Open terminal 1:
```bash
sudo dmesg -w
```

Open terminal 2 and perform operations:
```bash
sudo cat /proc/my_proc_device
echo "test" | sudo tee /proc/my_proc_device
```

### Use strace to See System Calls
```bash
sudo strace cat /proc/my_proc_device
```

### File Offset Tracking
```bash
# Check initial position
exec 3< /proc/my_proc_device
bash -c 'cat >&3' <<< "Hello"
exec 3<&-
```

---

## Cleanup

### Remove the Module
```bash
sudo rmmod trial
```

Verify it's removed:
```bash
lsmod | grep trial
```

### Clean Build Files
```bash
make clean
```

---

## Troubleshooting

### Module won't insert (Permission denied)
Run with sudo:
```bash
sudo insmod trial.ko
```

### /proc/my_proc_device not found
Verify module is loaded:
```bash
sudo dmesg | tail -10
```

### Permission denied when reading/writing
The module creates the file with 0666 permissions (read/write for all).
If you can't access it, try with sudo:
```bash
sudo cat /proc/my_proc_device
```

### Compilation errors
Make sure kernel headers are installed:
```bash
sudo apt-get install linux-headers-$(uname -r)
```

---

## Summary of Commands

```bash
# Compile
cd /home/desd/chetan_kotrange/LDD_Work/ex/
make

# Insert module
sudo insmod trial.ko

# Verify
lsmod | grep trial

# Test READ
sudo cat /proc/my_proc_device

# Test WRITE
echo "data" | sudo tee /proc/my_proc_device

# Test LSEEK
sudo dd if=/proc/my_proc_device bs=1 skip=5 count=10 2>/dev/null

# View logs
sudo dmesg | tail -20

# Remove module
sudo rmmod trial

# Clean
make clean
```

---

## File Operations Structure (proc_ops)

```c
static const struct proc_ops proc_fops = {
    .proc_open    = my_open,
    .proc_read    = my_read,
    .proc_write   = my_write,
    .proc_lseek   = my_lseek,
    .proc_release = my_release,
};
```

This structure ties all the operations together for the proc file system.


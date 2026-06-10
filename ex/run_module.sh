#!/bin/bash

# LDD Proc File System Module - Compilation and Execution Guide

echo "=========================================="
echo "LDD Proc File System Module"
echo "=========================================="
echo ""

# Check if we're running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Some operations require root privileges. Using sudo..."
fi

# Step 1: Compile the module
echo "[1] Compiling the kernel module..."
make clean
make

if [ ! -f "trial.ko" ]; then
    echo "ERROR: Compilation failed. trial.ko not found."
    exit 1
fi

echo "✓ Compilation successful!"
echo ""

# Step 2: Insert the module
echo "[2] Inserting the kernel module..."
sudo insmod trial.ko

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to insert module."
    exit 1
fi

echo "✓ Module inserted successfully!"
echo ""

# Step 3: Verify module is loaded
echo "[3] Verifying module is loaded..."
lsmod | grep trial

echo ""

# Step 4: Display kernel messages
echo "[4] Recent kernel messages:"
sudo dmesg | tail -10

echo ""

# Step 5: Create proc device file information
echo "[5] Proc device file info:"
echo "Location: /proc/my_proc_device"
echo "Permissions: 0666 (read/write for all users)"
echo ""

# Step 6: Test operations
echo "[6] Testing operations..."
echo ""

# Test READ
echo "Testing READ operation:"
sudo cat /proc/my_proc_device
echo ""

# Test WRITE
echo "Testing WRITE operation (writing new content):"
sudo bash -c 'echo "New data written to proc device" > /proc/my_proc_device'
echo "✓ Write successful!"
echo ""

# Verify the write
echo "Verifying written content:"
sudo cat /proc/my_proc_device
echo ""

# Test LSEEK
echo "Testing LSEEK operation:"
echo "Output with lseek (reading bytes 6-20):"
sudo bash -c 'dd if=/proc/my_proc_device bs=1 skip=6 count=15 2>/dev/null'
echo ""

echo "[7] Check kernel messages for detailed logs:"
echo "Command: sudo dmesg | tail -20"
echo ""

echo "=========================================="
echo "Testing completed!"
echo "=========================================="
echo ""
echo "To remove the module, run:"
echo "  sudo rmmod trial"
echo ""

#!/bin/bash
set -e

gcc -Wall -o ioctl_user ioctl_user.c

echo "Built ioctl_user"

echo "Run with: sudo ./ioctl_user" 

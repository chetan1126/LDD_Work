#ifndef IOCTL_CMD_H
#define IOCTL_CMD_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define MY_IOCTL_MAGIC 'N'
#define MY_IOCTL_RESET _IO(MY_IOCTL_MAGIC,0)
#define MY_IOCTL_SET_VALUE _IOW(MY_IOCTL_MAGIC,1,int)
#define MY_IOCTL_GET_VALUE _IOR(MY_IOCTL_MAGIC,2,int)
#define MY_IOCTL_TOGGLE_VALUE _IO(MY_IOCTL_MAGIC,3)

#endif


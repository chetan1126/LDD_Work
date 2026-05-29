#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>


#include "ioctl_cmd.h"

#define DEVICE_FILE  "/dev/file_operations"

int main(void)
{
	int fd;
	int value;

	char write_buf[] = "Hello from user space\n";
	char read_buf[128];
	ssize_t ret;

	fd = open (DEVICE_FILE, O_RDWR);
	if(fd < 0)
	{
		perror("open");
		return 1;
	}

	if(ioctl(fd,MY_IOCTL_RESET) < 0)
	{
		perror("ioctl reset");
		close(fd);
		return 1;
	}

	printf("User: ioctl RESET done \n");
	value = 25;

	if(ioctl(fd, MY_IOCTL_SET_VALUE, &value) < 0)
	{
		perror("ioctl set value\n");
		close(fd);
		return 1;
	}

	printf("User: ioctl SET_VALUE done, sent value = %d\n", value);
	value = 0;

	if(ioctl(fd, MY_IOCTL_GET_VALUE, &value) < 0)
	{
		perror("ioctl get value");
		close(fd);
		return 1;
	}

	printf("user: ioctl GET_VALUE returned value = %d\n", value);
	if(ioctl(fd, MY_IOCTL_TOGGLE_VALUE) < 0)
	{
		perror("ioctl toggle value");
		close(fd);
		return 1;
	}

	printf("User: ioctl TOGGLE_VALUE done \n");
	value = 0;
	if(ioctl(fd, MY_IOCTL_GET_VALUE, &value) < 0)
	{
		perror("ioctl get value after toggle");
		close(fd);
		return 1;
	}
	printf("User: after toggle, value = %d\n", value);
	
	close(fd);



	return 0;
}

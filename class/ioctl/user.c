#include <stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>

#include"ioctl.h"

#define DEVICE_FILE "/dev/ioctl_char"

int main(void)
{

	int fd;
	int value1 = 50;
	int ret;
	int value = value1;
	fd = open(DEVICE_FILE, O_RDWR);
	
	if(fd < 0)
	{
		perror("Open:");
		close(fd);
		return -1;
	}
//------------------------------------------------------------
	value = value1;
	ret = ioctl(fd,MY_IOCTL_RESET,&value);
	
	if(ret<0)
	{
		perror("Reset:");
                close(fd);
                return -1;

	}
	memset(&value, 0 , sizeof(value));
	
//---------------------------------------------------------
	value = 60;
	ret = ioctl(fd,MY_IOCTL_SET_VALUE ,&value);

        if(ret<0)
        {
                perror("Set:");
                close(fd);
                return -1;

        }
        memset(&value, 0 , sizeof(value));
//--------------------------------------------------------------
	//value = value1;
	ret = ioctl(fd,MY_IOCTL_GET_VALUE ,&value);

        if(ret<0)
        {
                perror("Get:");
                close(fd);
                return -1;

        }
        printf("MY_IOCTL_GET_VALUE from kernel = %d\n",value);

	memset(&value, 0 , sizeof(value));
//------------------------------------------------------------------
	value = 0;
	/*ret = ioctl(fd,MY_IOCTL_RESET,&value);
	
	if(ret<0)
	{
		perror("Reset:");
                close(fd);
                return -1;

	}
	memset(&value, 0 , sizeof(value));
	*/
	printf("before: MY_IOCTL_TOGGLE_VALUE from kernel = %d\n",value);
	ret = ioctl(fd,MY_IOCTL_TOGGLE_VALUE ,&value);

        if(ret<0)
        {
                perror("toggle:");
                close(fd);
                return -1;

        }
        printf("after: MY_IOCTL_TOGGLE_VALUE from kernel = %d\n",value);
	memset(&value, 0 , sizeof(value));

}

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

//--------------------------------------------------------------
	
	//-------------------------------------------
	ret = ioctl(fd,MY_IOCTL_GET_STRUCT ,&value);

        if(ret<0)
        {
                perror("Get:");
                close(fd);
                return -1;

        }
        printf("MY_IOCTL_GET_VALUE from kernel = %d\n",value);

	memset(&value, 0 , sizeof(value));
//------------------------------------------------------------------
	

}

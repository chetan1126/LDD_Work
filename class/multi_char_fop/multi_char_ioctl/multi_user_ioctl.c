#include <stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>

#include"ioctl.h"

#define DEVICE_FILE "/dev/multi_char3"

struct user_data{
	int id;
	char name[100];
	float marks;
	//write struct
};

struct user_read_data{
	int id;
	char name[100];
	float marks;
	//read struct
};

int main(void)
{

	int fd;
	int value1 = 50;
	int ret;
	int value = value1;
	struct user_data data;
	struct user_read_data read_data;
	fd = open(DEVICE_FILE, O_RDWR);
	
	if(fd < 0)
	{
		perror("Open:");
		close(fd);
		return -1;
	}
//---------------------READ ENABLE---------------------------------------
	value = value1;
	ret = ioctl(fd,MY_IOCTL_ENABLE);
	
	if(ret<0)
	{
		perror("Reset:");
                close(fd);
                return -1;

	}
	memset(&value, 0 , sizeof(value));
//---------------------------write---------------------------------------------------
	data.id =1;
	strcpy(data.name,"chetan kotrange");
	data.marks = 99.99;
	
	ret = ioctl(fd,MY_IOCTL_SET_VALUE ,&data);

        if(ret<0)
        {
                perror("Set:");
                close(fd);
                return -1;

        }
        memset(&value, 0 , sizeof(value));
//------------------------raed structure---------------------------------
	//value = value1;
	ret = ioctl(fd,MY_IOCTL_GET_VALUE ,&read_data);

        if(ret<0)
        {
                perror("Get:");
                close(fd);
                return -1;

        }
        printf("MY_IOCTL_GET_VALUE from kernel = %d\n",ret);
        printf("id=%d name=%s ,marks=%f\n",read_data.id,read_data.name,read_data.marks);

	memset(&value, 0 , sizeof(value));

//---------------------READ disable---------------------------------------
	value = value1;
	ret = ioctl(fd,MY_IOCTL_DISABLE);
	
	if(ret<0)
	{
		perror("Reset:");
                close(fd);
                return -1;

	}
	memset(&value, 0 , sizeof(value));
	
//---------------------------------------------------------
	data.id =2;
	strcpy(data.name,"kunal kotrange");
	data.marks = 99.99;
	
	if(ioctl(fd,MY_IOCTL_SET_VALUE ,&data)<0)
	{
                perror("Get:");
                close(fd);
                return -1;
        }
	if(ioctl(fd,MY_IOCTL_GET_VALUE ,&read_data)<0)
        {
                perror("Get:");
                close(fd);
                return -1;

        }
        printf("After read disable\n id=%d name=%s ,marks=%f\n", read_data.id, read_data.name, read_data.marks);
        //-------------------------------------------
/*	value = 2;
	ret = ioctl(fd,MY_IOCTL_SET_VALUE ,&value);

        if(ret<0)
        {
                perror("Set:");
                close(fd);
                return -1;

        }
        memset(&value, 0 , sizeof(value));
        
 */       
//--------------------------------------------------------------
/*	//value = value1;
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
	ret = ioctl(fd,MY_IOCTL_RESET,&value);
	
	if(ret<0)
	{
		perror("Reset:");
                close(fd);
                return -1;

	}
	memset(&value, 0 , sizeof(value));
	
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
*/
}

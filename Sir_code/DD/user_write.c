#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>

int main()
{
	int fd;
	char message [] = "HELLO KERNEL DRIVER";

	fd = open("/dev/write_operations", O_WRONLY);

	if(fd < 0)
	{
		perror("Open");
		return -1;
	}

	write(fd, message, strlen(message));

	printf("Data sent to Driver\n");
	close(fd);
	return 0;


}

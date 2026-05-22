// User space code

#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>

int main()
{

	int fd;
	char buffer[100]= "Hello from the user space\n";

	fd = open("/dev/write_fops", O_WRONLY);

	if(fd < 0)
	{
		perror("Open");
		return -1;
	}

	write(fd, buffer, sizeof(buffer));

	printf("Data from user:\n%s\n", buffer);
	close(fd);
	return 0;

}

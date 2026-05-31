#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>

int main()
{
	int fd;
	char buffer[100];

	fd = open("/dev/file_operations", O_RDONLY);
	if(fd < 0)
	{
		perror("Open");
		return -1;
	}

	read(fd, buffer, sizeof(buffer));
	printf("Data from Driver:\n%s\n", buffer);
	close(fd);
	return 0;


}

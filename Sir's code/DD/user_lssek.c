#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_FILE "/dev/lseek_char"

int main()
{
	int fd;
	char buf [32];
	ssize_t ret;
	off_t pos;

	fd = open(DEVICE_FILE, O_RDONLY);
	if(fd < 0)
	{
		perror("open");
		return 1;
	}
	memset(buf, 0, sizeof(buf));
	ret = read(fd, buf, 20);
	if (ret < 0)
	{
		perror("read");
		close(fd);
		return 1;
	}

	buf[ret] = '\0';
	printf("Read 1: %s\n", buf);
	pos = lseek(fd, 10, SEEK_CUR);
	if(pos < 0)
	{
		perror("lseek SEEK_CUR");
		close(fd);
		return 1;
	}

	printf("After SEEK_CUR, position= %ld\n", pos);
	memset(buf, 0, sizeof(buf));
	ret = read(fd, buf, 20);

	if(ret < 0)
	{
		perror("read");
		close(fd);
		return 1;
	}

	buf[ret] = '\0';
	printf("Read 2: %s\n", buf);
	pos = lseek(fd, 7, SEEK_SET);

	if(pos < 0)
	{
		perror("lseek SEEK_SET\n");
		close(fd);
		return 1;
	}
	printf("After SEEK_SET, position = %ld\n", pos);

	memset(buf, 0, sizeof(buf));
	ret = read(fd, buf, 20);

	if(ret < 0)
	{
		perror("read");
		close(fd);
		return 1;
	}

	buf[ret] = '\0';
	printf("Read 3: %s \n", buf);
	pos = lseek(fd, -10, SEEK_END);
	if(pos < 0)
	{
		perror("lseek SEEK_END");
		close(fd);
		return 1;
	}

	printf("AFter SEEK_END, position = %ld\n", pos);
	memset(buf, 0, sizeof(buf));
	ret = read(fd, buf, 10);

	if(ret < 0)
	{
		perror("read");
		close(fd);
		return 1;
	}
	buf[ret] = '\0';
	printf("Read 4: %s\n", buf);
	
	close(fd);
	return 0;

}

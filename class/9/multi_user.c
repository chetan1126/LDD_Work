#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<stdlib.h>

int main()
{
        int fd;
        char write_buf[] = "CHETAN KOTRANGE";
        char read_buf[50];
        off_t pos;
        int ret;

        //--------------------------------------------------------------------------------Open device 

        fd = open("/dev/multi_char0", O_RDWR);

        if(fd < 0)
        {
                perror("open");
                return 1;
        }

        printf("Device opened successfully\n");

//----------------------------------------------------------------------------------------------------------Write data 

        ret = write(fd, write_buf, strlen(write_buf));

        if(ret < 0)
        {
                perror("write");
                close(fd);
                return 1;
        }

        printf("Written Data : %s\n", write_buf);

//-----------------------------------------------------------------Move pointer to beginning

        pos = lseek(fd, 0, SEEK_SET);

        if(pos < 0)
        {
                perror("lseek SEEK_SET");
                close(fd);
                return 1;
        }

        printf("Current Position after SEEK_SET = %ld\n", pos);

//------------------------------------------------------------------------------------------Read first 5 byte

        memset(read_buf, 0, sizeof(read_buf));

        ret = read(fd, read_buf, 5);
	read_buf[ret] = '\0';
        if(ret < 0)
        {
                perror("read");
                close(fd);
                return 1;
        }

        printf("Read Data = %s\n", read_buf);

//------------------------------------------------------------------------------------------Move 2 bytes forward from current position

        pos = lseek(fd, 2, SEEK_CUR);

        if(pos < 0)
        {
                perror("lseek SEEK_CUR");
                close(fd);
                return 1;
        }

        printf("Current Position after SEEK_CUR = %ld\n", pos);

        //------------------------------------------------------------------------------------------Read again

        memset(read_buf, 0, sizeof(read_buf));

        ret = read(fd, read_buf, 5);
	read_buf[ret] = '\0';
        if(ret < 0)
        {
                perror("read");
                close(fd);
                return 1;
        }

        printf("Read Data = %s\n", read_buf);

        //------------------------------------------------------------------------------------------Move to end

        pos = lseek(fd, 0, SEEK_END);

        if(pos < 0)
        {
                perror("lseek SEEK_END");
                close(fd);
                return 1;
        }

        printf("Current Position after SEEK_END = %ld\n", pos);

        close(fd);

        return 0;
}

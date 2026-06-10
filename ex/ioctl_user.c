#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/ioctl_demo"

/* Same ioctl definitions as kernel */
#define IOCTL_MAGIC 'k'
#define IOCTL_GET_MSG   _IOR(IOCTL_MAGIC, 1, char *)
#define IOCTL_SET_MSG   _IOW(IOCTL_MAGIC, 2, char *)
#define IOCTL_GET_NUM   _IOR(IOCTL_MAGIC, 3, int *)
#define IOCTL_SET_NUM   _IOW(IOCTL_MAGIC, 4, int *)
#define IOCTL_EXCHANGE  _IOWR(IOCTL_MAGIC,5, struct ioctl_data *)

struct ioctl_data {
    int num;
    char msg[128];
};

int main(void)
{
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    /* Test SET_MSG */
    const char *new_msg = "Hello from userspace!";
    if (ioctl(fd, IOCTL_SET_MSG, new_msg) < 0) {
        perror("IOCTL_SET_MSG");
    } else {
        printf("IOCTL_SET_MSG sent: %s\n", new_msg);
    }

    /* Test GET_MSG */
    char buf[128] = {0};
    if (ioctl(fd, IOCTL_GET_MSG, buf) < 0) {
        perror("IOCTL_GET_MSG");
    } else {
        printf("IOCTL_GET_MSG received: %s\n", buf);
    }

    /* Test SET_NUM */
    int num = 42;
    if (ioctl(fd, IOCTL_SET_NUM, &num) < 0) {
        perror("IOCTL_SET_NUM");
    } else {
        printf("IOCTL_SET_NUM sent: %d\n", num);
    }

    /* Test GET_NUM */
    int ret_num = 0;
    if (ioctl(fd, IOCTL_GET_NUM, &ret_num) < 0) {
        perror("IOCTL_GET_NUM");
    } else {
        printf("IOCTL_GET_NUM received: %d\n", ret_num);
    }

    /* Test EXCHANGE */
    struct ioctl_data data;
    data.num = 777;
    strncpy(data.msg, "User provided message", sizeof(data.msg)-1);
    data.msg[sizeof(data.msg)-1] = '\0';

    printf("IOCTL_EXCHANGE sending num=%d msg=\"%s\"\n", data.num, data.msg);
    if (ioctl(fd, IOCTL_EXCHANGE, &data) < 0) {
        perror("IOCTL_EXCHANGE");
    } else {
        /* After exchange, data now contains the previous kernel values */
        printf("IOCTL_EXCHANGE returned old num=%d old msg=\"%s\"\n", data.num, data.msg);
    }

    /* Read device with read() */
    lseek(fd, 0, SEEK_SET);
    ssize_t r = read(fd, buf, sizeof(buf)-1);
    if (r > 0) {
        buf[r] = '\0';
        printf("read() after exchange -> %s\n", buf);
    }

    close(fd);
    return 0;
}

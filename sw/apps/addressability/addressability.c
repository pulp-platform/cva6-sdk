#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

int main(int argc, char *argv[]) {
    char *device_path;
    uint8_t *soc_ctrl = NULL;
    int device_fd;

    if (argc != 2) {
        printf("Wrong usage : %s device\n", argv[0]);
        return -1;
    }

    device_path = argv[1];

    printf("Starting addressability test with %s\n", device_path);

    device_fd = open(device_path, O_RDWR | O_SYNC);

    soc_ctrl =
        mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, device_fd, 0);

    if (soc_ctrl == MAP_FAILED) {
        printf("mmap() failed %s\n", strerror(errno));
        return -EIO;
    }

    printf("mmap() success at %p\n", soc_ctrl);

    printf("Scratch regs : (0=%x; 1=%x)\n", ((uint32_t *)soc_ctrl)[6], ((uint32_t *)soc_ctrl)[7]);
    printf("Resets : (host=%x; periph=%x; safety=%x; security=%x; pulp=%x; spatz=%x; l2=%x)\n", ((uint32_t *)soc_ctrl)[8], ((uint32_t *)soc_ctrl)[9], ((uint32_t *)soc_ctrl)[10], ((uint32_t *)soc_ctrl)[11], ((uint32_t *)soc_ctrl)[12], ((uint32_t *)soc_ctrl)[13], ((uint32_t *)soc_ctrl)[14]);

    sleep(1);

    munmap(soc_ctrl, 0x1000);
    close(device_fd);

    return 0;
}
#include "sut.h"
#include <stdio.h>
#include <string.h>

void hello1() {
    int i, fd;
    char sbuf[128];
    fd = sut_open("./test4.txt");
    if (fd < 0)
        printf("Error: sut_open() failed\n");
    else {
        for (i = 0; i < 100; i++) {
            sprintf(sbuf, "Hello world!, message from SUT-One i = %d \n", i);
            sut_write(fd, sbuf, strlen(sbuf));
            sut_yield();
        }
        sut_close(fd);
    }
    sut_exit();
}

void hello2() {
    int i;
    for (i = 0; i < 100; i++) {
        printf("Hello world!, this is SUT-Two \n");
        sut_yield();
    }
    sut_exit();
}

int main() {
    sut_init();
    sut_create(hello1);
    sut_create(hello2);
    sut_shutdown();
}

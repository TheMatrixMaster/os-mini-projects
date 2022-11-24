#ifndef __SUT_H__
#define __SUT_H__

#include <stdbool.h>

#define MAX_THREADS                 32
#define THREAD_STACK_SIZE           1024*1024

typedef void (*sut_task_f)();

void sut_init();
bool sut_create(sut_task_f fn);
void sut_yield();
void sut_exit();
int sut_open(char *fname);
void sut_write(int fd, char *buf, int size);
void sut_close(int fd);
char *sut_read(int fd, char *buf, int size);
void sut_shutdown();


#endif

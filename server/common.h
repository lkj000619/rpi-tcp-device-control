#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
// 조도(LDR): %3d | 온도(Therm): %3f | 가변저항(Pot): %3d | LED(D1): %d
typedef struct {
    int read_ldr;
    float read_therm;
    int read_pot;
    int yl40_fd;

    pthread_mutex_t *mutex;
} readIO_t;

#endif
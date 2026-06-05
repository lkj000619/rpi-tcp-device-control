#include <wiringPi.h>
#include <softPwm.h>
#include <softTone.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "io_control.h"
#include "server_socket.h" // write_daemon_log, client_session_t, send_menu_and_prompt 참조용
#include <string.h>

// 세그먼트 디코더 핀 mapping 및 숫자 비트 정의
static const int fnd_pins[4] = {FND_D_PIN, FND_C_PIN, FND_B_PIN, FND_A_PIN}; 
static const int fnd_digits[10][4] = {
    {0,0,0,0}, // 0
    {0,0,0,1}, // 1
    {0,0,1,0}, // 2
    {0,0,1,1}, // 3
    {0,1,0,0}, // 4
    {0,1,0,1}, // 5
    {0,1,1,0}, // 6
    {0,1,1,1}, // 7
    {1,0,0,0}, // 8
    {1,0,0,1}  // 9
};

// 부저 연주 음표
static const int buzzer_notes[] = {
    1200, 800, 1200, 800, 1200, 800, 1200, 800, 1200, 800,
    1200, 800, 1200, 800, 1200, 800, 1200, 800, 1200, 800
};

// 비동기 제어 리소스 및 뮤텍스
static pthread_t led_threads[4]; // 1: LED1, 2: LED2, 3: LED3, 0: YL40 D1 LED
static int led_thread_running[4] = {0};
static pthread_mutex_t led_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t buz_thread;
static int buz_thread_running = 0;
static pthread_mutex_t buz_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t seg_thread;
static int seg_thread_running = 0;
static pthread_mutex_t seg_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

void stop_led_thread(int index) {
    pthread_mutex_lock(&led_thread_mutex);
    if (led_thread_running[index]) {
        pthread_cancel(led_threads[index]);
        pthread_join(led_threads[index], NULL);
        led_thread_running[index] = 0;
    }
    pthread_mutex_unlock(&led_thread_mutex);
}

void stop_buz_thread() {
    pthread_mutex_lock(&buz_thread_mutex);
    if (buz_thread_running) {
        pthread_cancel(buz_thread);
        pthread_join(buz_thread, NULL);
        buz_thread_running = 0;
    }
    pthread_mutex_unlock(&buz_thread_mutex);
    softToneWrite(BUZ_PIN, 0);
}

void stop_seg_thread() {
    pthread_mutex_lock(&seg_thread_mutex);
    if (seg_thread_running) {
        pthread_cancel(seg_thread);
        pthread_join(seg_thread, NULL);
        seg_thread_running = 0;
    }
    pthread_mutex_unlock(&seg_thread_mutex);
    for (int i = 0; i < 4; i++) {
        digitalWrite(fnd_pins[i], HIGH);
    }
}

void stop_all_io_operations() {
    for (int led = 0; led <= 3; led++) {
        stop_led_thread(led);
    }
    stop_buz_thread();
    stop_seg_thread();
}

void start_led_blink(int led_num, int target_pin, int is_soft) {
    stop_led_thread(led_num);
    pthread_mutex_lock(&led_thread_mutex);
    LedEffectArg *arg = malloc(sizeof(LedEffectArg));
    arg->pin = target_pin;
    arg->is_soft = is_soft;
    pthread_create(&led_threads[led_num], NULL, led_blink_thread_func, arg);
    led_thread_running[led_num] = 1;
    pthread_mutex_unlock(&led_thread_mutex);
}

void start_led_fade(int led_num, int target_pin, int is_soft) {
    stop_led_thread(led_num);
    pthread_mutex_lock(&led_thread_mutex);
    LedEffectArg *arg = malloc(sizeof(LedEffectArg));
    arg->pin = target_pin;
    arg->is_soft = is_soft;
    pthread_create(&led_threads[led_num], NULL, led_fade_thread_func, arg);
    led_thread_running[led_num] = 1;
    pthread_mutex_unlock(&led_thread_mutex);
}

void start_yl40_d1_blink(int yl40_fd) {
    stop_led_thread(0);
    pthread_mutex_lock(&led_thread_mutex);
    Yl40D1BlinkArg *d1_arg = malloc(sizeof(Yl40D1BlinkArg));
    d1_arg->fd = yl40_fd;
    pthread_create(&led_threads[0], NULL, yl40_d1_blink_thread_func, d1_arg);
    led_thread_running[0] = 1;
    pthread_mutex_unlock(&led_thread_mutex);
}

void start_buz_melody() {
    stop_buz_thread();
    pthread_mutex_lock(&buz_thread_mutex);
    pthread_create(&buz_thread, NULL, buz_play_thread_func, NULL);
    buz_thread_running = 1;
    pthread_mutex_unlock(&buz_thread_mutex);
}

void start_seg_timer(int seconds, int clnt_fd, struct client_session_t *sess, readIO_t *readIO) {
    stop_seg_thread();
    pthread_mutex_lock(&seg_thread_mutex);
    SegArg *s_arg = malloc(sizeof(SegArg));
    s_arg->start_num = seconds;
    s_arg->client_fd = clnt_fd;
    s_arg->session = sess;
    s_arg->readIO = readIO;
    pthread_create(&seg_thread, NULL, seg_thread_func, s_arg);
    seg_thread_running = 1;
    pthread_mutex_unlock(&seg_thread_mutex);
}

void *led_blink_thread_func(void *arg) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    LedEffectArg effect_arg = *(LedEffectArg *)arg;
    free(arg); 
    
    int pin = effect_arg.pin;
    int is_soft = effect_arg.is_soft;
    int state = 0;

    if (is_soft) {
        while (1) {
            state = !state;
            softPwmWrite(pin, state ? 255 : 0);
            usleep(500000);
        }
    } else {
        while (1) {
            state = !state;
            pwmWrite(pin, state ? 1023 : 0);
            usleep(500000);
        }
    }
    return NULL;
}

void *led_fade_thread_func(void *arg) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    LedEffectArg effect_arg = *(LedEffectArg *)arg;
    free(arg); 
    
    int pin = effect_arg.pin;
    int is_soft = effect_arg.is_soft;

    if (is_soft) {
        while (1) {
            for (int val = 0; val <= 255; val += 5) {
                softPwmWrite(pin, val);
                usleep(15000);
            }
            for (int val = 255; val >= 0; val -= 5) {
                softPwmWrite(pin, val);
                usleep(15000);
            }
        }
    } else {
        while (1) {
            for (int val = 0; val <= 1023; val += 20) {
                pwmWrite(pin, val);
                usleep(15000);
            }
            for (int val = 1023; val >= 0; val -= 20) {
                pwmWrite(pin, val);
                usleep(15000);
            }
        }
    }
    return NULL;
}

void *yl40_d1_blink_thread_func(void *arg) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    Yl40D1BlinkArg d1_arg = *(Yl40D1BlinkArg *)arg;
    free(arg); 
    
    int fd = d1_arg.fd;
    int state = 0;
    while (1) {
        state = !state;
        write_pcf8591(fd, YL40_AOUT, state ? 255 : 0);
        usleep(1000000);
    }
    return NULL;
}

void *buz_play_thread_func(void *arg) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    (void)arg;
    softToneCreate(BUZ_PIN);
    while (1) {
        for (int i = 0; i < 20; i++) {
            softToneWrite(BUZ_PIN, buzzer_notes[i]);
            usleep(280000);
        }
    }
    return NULL;
}

void *seg_thread_func(void *arg) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    SegArg s_arg = *(SegArg *)arg;
    free(arg); 
    
    int current = s_arg.start_num;
    int clnt_fd = s_arg.client_fd;
    client_session_t *sess = (client_session_t *)s_arg.session;
    readIO_t *readIO = s_arg.readIO;

    for (int i = 0; i < 4; i++) {
        pinMode(fnd_pins[i], OUTPUT);
    }

    char msg[256];
    for (int i = current; i >= 0; i--) {
        for (int j = 0; j < 4; j++) {
            digitalWrite(fnd_pins[j], fnd_digits[i][j] ? HIGH : LOW);
        }
        sprintf(msg, "\n[백그라운드] 가상 7-Segment 표시: %d초\n", i);
        write(clnt_fd, msg, strlen(msg));
        if (i == 0) break;
        sleep(1);
    }

    sprintf(msg, "\n[백그라운드] 타이머 종료! 가상 부저 ON\n");
    write(clnt_fd, msg, strlen(msg));
    write_daemon_log("INFO", "[Socket: %d] 7-Segment 제어: 타이머 완료 및 알림 경보 구동\n", clnt_fd);

    softToneCreate(BUZ_PIN);
    softToneWrite(BUZ_PIN, 1000);
    usleep(1500000); 
    softToneWrite(BUZ_PIN, 0);

    for (int i = 0; i < 4; i++) {
        digitalWrite(fnd_pins[i], HIGH);
    }

    sess->current_session = SESSION_MAIN;
    send_menu_and_prompt(clnt_fd, sess, readIO);

    pthread_mutex_lock(&seg_thread_mutex);
    seg_thread_running = 0;
    pthread_mutex_unlock(&seg_thread_mutex);

    write_daemon_log("INFO", "[Socket: %d] 7-Segment 제어: 경보음 종료 및 7-Segment 소등 완료\n", clnt_fd);
    return NULL;
}

#ifndef IO_CONTROL_H
#define IO_CONTROL_H

#include <pthread.h>

// 핀 매핑 (wiringPi 번호 기준)
#define LED1_PIN 22  // GPIO 6 (Soft PWM)
#define LED2_PIN 26  // GPIO 12 (Hard PWM)
#define LED3_PIN 23  // GPIO 13 (Hard PWM)
#define BUZ_PIN  21  // GPIO 5

// YL-40 PCF8591 I2C 주소 및 채널 매핑
#define YL40_I2C_ADDR 0x48
#define YL40_AIN0 0x00 // LDR (조도 센서)
#define YL40_AIN1 0x01 // Thermistor (온도 센서)
#define YL40_AIN2 0x02 // 외부 입력 핀
#define YL40_AIN3 0x03 // Potentiometer (가변 저항)
#define YL40_AOUT 0x40 // D1 LED (DAC 출력)

// 7-Segment 디코더용 핀 정의 (D, C, B, A 순)
#define FND_D_PIN 4  // GPIO 23
#define FND_C_PIN 3  // GPIO 22
#define FND_B_PIN 2  // GPIO 27
#define FND_A_PIN 0  // GPIO 17

// 조도(LDR): %3d | 온도(Therm): %3f | 가변저항(Pot): %3d | LED(D1): %d
typedef struct {
    int read_ldr;
    float read_therm;
    int read_pot;
    int yl40_fd;

    int auto_mode;           // 조도 자동 제어 모드 ON(1)/OFF(0)
    int threshold;           // 조도 판단 기준값 (0 ~ 255)
    int d1_led_state;        // DAC D1 LED 현재 물리적 출력 상태 밝기값 (0 ~ 255)
    int d1_led_preset;       // DAC D1 LED 사용자가 설정한 목표 밝기 설정값 (0 ~ 255)
    char log_filename[256];  // 로그 파일명 저장 필드
    int led_pwm_state[4];    // 3색 LED 현재 물리적 출력 상태 밝기값 (1~3번, 꺼짐=0, 켜짐>0)
    int led_pwm_preset[4];   // 3색 LED 사용자가 설정한 목표 밝기 설정값 (1~3번, 0~255)
    int active_client_fd;    // 출력 장치 제어를 점유한 클라이언트 소켓 FD (-1 이면 미점유)

    pthread_mutex_t *mutex;
} readIO_t;

// 전방 선언
struct client_session_t;

typedef struct {
    int pin;
    int is_soft;
} LedEffectArg;

typedef struct {
    int fd;
} Yl40D1BlinkArg;

typedef struct {
    int start_num;
    int client_fd;
    struct client_session_t *session;
    readIO_t *readIO;
} SegArg;

// IO 제어 헬퍼 함수 원형
void stop_led_thread(int index);
void stop_buz_thread();
void stop_seg_thread();
void stop_all_io_operations();

// 비동기 스레드 실행을 캡슐화한 인터페이스 함수 원형
void start_led_blink(int led_num, int target_pin, int is_soft);
void start_led_fade(int led_num, int target_pin, int is_soft);
void start_yl40_d1_blink(int yl40_fd);
void start_buz_melody();
void start_seg_timer(int seconds, int clnt_fd, struct client_session_t *sess, readIO_t *readIO);

// 비동기 스레드 구동 실제 함수
void *led_blink_thread_func(void *arg);
void *led_fade_thread_func(void *arg);
void *buz_play_thread_func(void *arg);
void *seg_thread_func(void *arg);
void *yl40_d1_blink_thread_func(void *arg);

int yl40_setup();
int read_pcf8591(int fd, int channel);
void write_pcf8591(int fd, int channel, int value);
double convert_to_celsius(int raw_adc);
void IOread(readIO_t* readIO);

#endif

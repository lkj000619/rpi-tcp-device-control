// yl-40 PCF8591 ADC Module control function
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <unistd.h>
#include "common.h"

// NTC 써미스터 계산용 상수 (일반적인 YL-40 10k NTC 기준)
#define B_COEFFICIENT 3950.0        // 써미스터의 B 파라미터 값
#define NOMINAL_RESISTANCE 100000.0  // 기준 온도에서의 저항 (100k옴)
#define NOMINAL_TEMPERATURE 298.15  // 기준 온도 25℃를 켈빈(K)으로 변환
#define SERIES_RESISTOR 10000.0     // 분압 회로에 연결된 직렬 저항 (10k옴)

int yl40_setup();
int read_pcf8591(int fd, int channel);
void write_pcf8591(int fd, int channel, int value);
double convert_to_celsius(int raw_adc);

void IOread(readIO_t* readIO){
    readIO->yl40_fd = yl40_setup();
    if(readIO->yl40_fd < 0) {
        while(1) { sleep(1); }
    }

    // 기본 임계값 및 PWM 상태 초기화
    pthread_mutex_lock(readIO->mutex);
    readIO->threshold = 180;
    readIO->auto_mode = 0;
    readIO->d1_led_state = 0;
    readIO->led_pwm_state[0] = 0;
    readIO->led_pwm_state[1] = 0;
    readIO->led_pwm_state[2] = 0;
    readIO->led_pwm_state[3] = 0;
    pthread_mutex_unlock(readIO->mutex);

    // 초기 DAC LED 리셋 (AOUT 채널 사용)
    write_pcf8591(readIO->yl40_fd, YL40_AOUT, 0);

    while(1){
        int     ldr      = read_pcf8591(readIO->yl40_fd, YL40_AIN0);
        int     pot      = read_pcf8591(readIO->yl40_fd, YL40_AIN3);
        int     temp_raw = read_pcf8591(readIO->yl40_fd, YL40_AIN1);
        double  temp     = convert_to_celsius(temp_raw);

        pthread_mutex_lock(readIO->mutex);
        readIO->read_ldr    = ldr;
        readIO->read_pot    = pot;
        readIO->read_therm  = (float)temp;

        // 조도 오토 모드 제어
        if (readIO->auto_mode == 1) {
            // 조도가 임계값 이상이면 어둠 -> D1 LED 켜기 (255)
            // 임계값 미만이면 밝음 -> D1 LED 끄기 (0)
            int target_led = (ldr >= readIO->threshold) ? 255 : 0;
            if (readIO->d1_led_state != target_led) {
                readIO->d1_led_state = target_led;
                write_pcf8591(readIO->yl40_fd, YL40_AOUT, readIO->d1_led_state);
                write_daemon_log("INFO", "오토 모드 트리거: 현재 조도(%d)가 임계값(%d) %s 상태가 되어 D1 LED를 %s했습니다.\n",
                                 ldr, readIO->threshold, (ldr >= readIO->threshold) ? "이상(어두움)" : "미만(밝음)", target_led ? "점등(ON)" : "소등(OFF)");
            }
        }
        pthread_mutex_unlock(readIO->mutex);

        usleep(100000); // 100ms
    }
}

int yl40_setup(){
    int fd;
    if (wiringPiSetup() == -1) {
        printf("wiringPiSetup failed:\n"); 
    }
    // 출력 핀들 기본 모드 설정
    pinMode(LED1_PIN, OUTPUT);
    softPwmCreate(LED1_PIN, 0, 255);
    
    pinMode(LED2_PIN, PWM_OUTPUT);
    pinMode(LED3_PIN, PWM_OUTPUT);
    
    // FND용 핀 4개 초기화
    pinMode(FND_D_PIN, OUTPUT);
    pinMode(FND_C_PIN, OUTPUT);
    pinMode(FND_B_PIN, OUTPUT);
    pinMode(FND_A_PIN, OUTPUT);
    
    // FND 초기 상태로 끄기 (HIGH)
    digitalWrite(FND_D_PIN, HIGH);
    digitalWrite(FND_C_PIN, HIGH);
    digitalWrite(FND_B_PIN, HIGH);
    digitalWrite(FND_A_PIN, HIGH);

    if((fd = wiringPiI2CSetupInterface("/dev/i2c-1", YL40_I2C_ADDR)) < 0) { 
        printf("wiringPiI2CSetupInterface failed:\n"); 
    }
    return fd;
}

int read_pcf8591(int fd, int channel){
    int control_byte = 0x00 | channel;
    wiringPiI2CWrite(fd, control_byte);     // 데이터 요청
    wiringPiI2CRead(fd);                    // 더미 리드 (이전 값 버림)
    return wiringPiI2CRead(fd);             // 실제 데이터 리드
}

void write_pcf8591(int fd, int channel, int value){
    int control_byte = 0x00 | channel;
    wiringPiI2CWriteReg8(fd, control_byte, value);
}

double convert_to_celsius(int raw_adc) {
    if (raw_adc <= 0) return -273.15;
    if (raw_adc >= 255) return 99.99;

    double resistance = SERIES_RESISTOR * (double)raw_adc / (255.0 - raw_adc);
    double temp_k = resistance / NOMINAL_RESISTANCE;     
    temp_k = log(temp_k);                                
    temp_k /= B_COEFFICIENT;                             
    temp_k += 1.0 / NOMINAL_TEMPERATURE;                 
    temp_k = 1.0 / temp_k;                               

    return temp_k - 273.15;
}

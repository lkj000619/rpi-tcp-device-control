// yl-40 PCF8591 ADC Module control function
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <unistd.h>
#include "io_control.h"
#include "server_socket.h"
#include <softPwm.h>

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
    set_daemon_log_filename(readIO->log_filename);
    readIO->yl40_fd = yl40_setup();
    if(readIO->yl40_fd < 0) {
        while(1) { sleep(1); }
    }

    // 기본 임계값 및 PWM 상태 초기화
    pthread_mutex_lock(readIO->mutex);
    readIO->threshold = 180;
    readIO->auto_mode = 0;
    readIO->d1_led_state = 0;
    readIO->d1_led_preset = 255;
    readIO->led_pwm_state[0] = 0;
    readIO->led_pwm_state[1] = 0;
    readIO->led_pwm_state[2] = 0;
    readIO->led_pwm_state[3] = 0;
    readIO->led_pwm_preset[0] = 255;
    readIO->led_pwm_preset[1] = 255;
    readIO->led_pwm_preset[2] = 255;
    readIO->led_pwm_preset[3] = 255;
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
        static int last_auto_state = -1;
        if (readIO->auto_mode == 1) {
            int current_state = (ldr >= readIO->threshold) ? 1 : 0;
            if (current_state != last_auto_state) {
                last_auto_state = current_state;
                
                // 데드락 예방을 위해 락 해제 후 스레드 조작
                pthread_mutex_unlock(readIO->mutex);
                
                stop_led_thread(1);
                stop_led_thread(2);
                stop_led_thread(3);
                
                pthread_mutex_lock(readIO->mutex);
                
                if (current_state == 1) {
                    int p1 = readIO->led_pwm_preset[1];
                    int p2 = readIO->led_pwm_preset[2];
                    int p3 = readIO->led_pwm_preset[3];
                    
                    softPwmWrite(LED1_PIN, p1);
                    pwmWrite(LED2_PIN, (p2 * 1023) / 255);
                    pwmWrite(LED3_PIN, (p3 * 1023) / 255);
                    
                    readIO->led_pwm_state[1] = p1;
                    readIO->led_pwm_state[2] = p2;
                    readIO->led_pwm_state[3] = p3;
                    
                    write_daemon_log("INFO", "오토 모드 트리거: 어두움 감지(조도:%d >= 임계값:%d) -> 3색 LED 동시 점등 (밝기 R:%d, Y:%d, G:%d)\n",
                                     ldr, readIO->threshold, p1, p2, p3);
                } else {
                    softPwmWrite(LED1_PIN, 0);
                    pwmWrite(LED2_PIN, 0);
                    pwmWrite(LED3_PIN, 0);
                    
                    readIO->led_pwm_state[1] = 0;
                    readIO->led_pwm_state[2] = 0;
                    readIO->led_pwm_state[3] = 0;
                    
                    write_daemon_log("INFO", "오토 모드 트리거: 밝음 감지(조도:%d < 임계값:%d) -> 3색 LED 동시 소등\n",
                                     ldr, readIO->threshold);
                }
            }
        } else {
            last_auto_state = -1; // 오토 모드 꺼져 있을 땐 상태 리셋
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
    int control_byte = 0x40 | channel;
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

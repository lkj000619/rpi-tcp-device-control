// yl-40 PCF8591 ADC Moudule control function
#include <pthread.h>
#include "common.h"


// NTC 써미스터 계산용 상수 (일반적인 YL-40 10k NTC 기준)
#define B_COEFFICIENT 3950.0        // 써미스터의 B 파라미터 값
#define NOMINAL_RESISTANCE 100000.0  // 기준 온도에서의 저항 (100k옴)
#define NOMINAL_TEMPERATURE 298.15  // 기준 온도 25℃를 켈빈(K)으로 변환
#define SERIES_RESISTOR 10000.0     // 분압 회로에 연결된 직렬 저항 (10k옴)

#define LDR 0x00
#define THERM 0x01
#define POT 0x03
#define LED 0x40

int yl40_setup();
int yl40_read(int fd, int channel);
double convert_to_celsius(int raw_adc);
void yl40_write(int fd, int pwm);

void IOread(readIO_t* readIO){
    readIO->yl40_fd = yl40_setup();

    while(1){
        int     ldr    = yl40_read(readIO->yl40_fd, LDR);
        int     pot    = yl40_read(readIO->yl40_fd, POT);
        double  temp   = convert_to_celsius(read_pcf8591(readIO->yl40_fd, THERM));

        pthread_mutex_lock(readIO->mutex);
        readIO->read_ldr    = ldr;
        readIO->read_pot    = pot;
        readIO->read_therm  = temp;
        pthread_mutex_unlock(readIO->mutex);
    }
}

int yl40_setup(){
    int fd;
    if((fd = wiringPiI2CSetupInterface("/dev/i2c-1",0x48))<0) { 
        printf("wiringPiI2CSetupInterface failed:\n"); 
    }
    return fd;
}

int yl40_read(int fd, int channel){
    int control_byte = 0x00 | channel;
    wiringPiI2CWrite(fd, control_byte);     // 데이터 요청
    wiringPiI2CRead(fd);                    // 데미 데이터
    
    return wiringPiI2CRead(fd);             // 데이터 리드
}

double convert_to_celsius(int raw_adc) {
    // 0으로 나누기 오류 방지 (안전 장치)
    if (raw_adc <= 0) return -273.15;
    if (raw_adc >= 255) return 99.99; // 측정 불가(최대치) 처리

    // 1. 써미스터의 현재 저항값(Rt) 계산
    // 모듈의 회로 구성에 따라 (255 - raw_adc) / raw_adc 또는 그 반대일 수 있습니다.
    // double resistance = SERIES_RESISTOR * (255.0 - raw_adc) / (double)raw_adc;
    double resistance = SERIES_RESISTOR * (double)raw_adc / (255.0 - raw_adc);

    // 2. B-파라미터 방정식을 이용해 켈빈(Kelvin) 온도 계산
    double temp_k = resistance / NOMINAL_RESISTANCE;     // (R / R0)
    temp_k = log(temp_k);                                // ln(R / R0)
    temp_k /= B_COEFFICIENT;                             // 1/B * ln(R / R0)
    temp_k += 1.0 / NOMINAL_TEMPERATURE;                 // + (1 / T0)
    temp_k = 1.0 / temp_k;                               // 역수 취하기

    // 3. 켈빈 온도를 섭씨 온도로 변환
    return temp_k - 273.15;
}

void yl40_write(int fd, int pwm){
    int control_byte = 0x00 | LED;
    wiringPiI2CWriteReg8(fd, control_byte, pwm);
}

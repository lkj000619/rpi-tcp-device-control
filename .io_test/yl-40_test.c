#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <unistd.h>
#include <math.h>

#define I2C_ADDR 0x48

// 채널별 매핑 (보편적인 YL-40 모듈 기준)
#define AIN0 0x00 // LDR (조도 센서)
#define AIN1 0x01 // Thermistor (온도 센서)
#define AIN2 0x02 // 외부 입력 핀
#define AIN3 0x03 // Potentiometer (가변 저항)
#define AOUT 0x40 // D1 LED

// NTC 써미스터 계산용 상수 (일반적인 YL-40 10k NTC 기준)
#define B_COEFFICIENT 3950.0        // 써미스터의 B 파라미터 값
#define NOMINAL_RESISTANCE 100000.0  // 기준 온도에서의 저항 (100k옴)
#define NOMINAL_TEMPERATURE 298.15  // 기준 온도 25℃를 켈빈(K)으로 변환
#define SERIES_RESISTOR 10000.0     // 분압 회로에 연결된 직렬 저항 (10k옴)

/*
 * PCF8591 I2C 데이터 읽기 함수
 */
int read_pcf8591(int fd, int channel) {
    // Control Byte 생성: 
    // 0x40은 아날로그 출력(DAC) 활성화 비트를 켠 기본 상태입니다.
    // 여기에 채널 번호(0~3)를 더해 어떤 채널을 읽을지 칩에 알려줍니다.
    int control_byte = 0x00 | channel;
    
    // 1. 해당 채널을 읽겠다고 I2C 컨트롤 바이트 전송
    wiringPiI2CWrite(fd, control_byte);
    
    // 2. Dummy Read: 이전 채널의 변환 결과가 남아있으므로 한 번 읽어서 버림
    wiringPiI2CRead(fd);
    
    // 3. Actual Read: 방금 요청한 채널의 실제 ADC 값을 반환
    return wiringPiI2CRead(fd);
}

void write_pcf8591(int fd, int channel, int value) {
    // Control Byte 생성: 
    // 0x40은 아날로그 출력(DAC) 활성화 비트를 켠 기본 상태입니다.
    // 여기에 채널 번호(0~3)를 더해 어떤 채널을 읽을지 칩에 알려줍니다.
    int control_byte = 0x00 | channel;
    
    // 1. 해당 채널을 읽겠다고 I2C 컨트롤 바이트 전송
    wiringPiI2CWriteReg8(fd, channel, value);
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

int main(void) {
    int fd;

    // wiringPi 초기화
    if (wiringPiSetup() == -1) {
        printf("wiringPi 초기화 실패\n");
        return 1;
    }

    // I2C 장치 초기화 (파일 디스크립터 반환)
    fd = wiringPiI2CSetup(I2C_ADDR);
    if (fd == -1) {
        printf("I2C 장치(0x48)를 찾을 수 없거나 초기화에 실패했습니다.\n");
        return 1;
    }

    printf("YL-40 센서 데이터 읽기 시작 (Raw I2C 모드)...\n");
    printf("종료하려면 Ctrl+C를 누르세요.\n\n");
    
    int led_value = 255;
    while(1) {
        // 각 채널별로 데이터를 순차적으로 읽어옵니다.
        // int ext_in = read_pcf8591(fd, AIN0);
        int ldr    = read_pcf8591(fd, AIN0);
        int temp   = read_pcf8591(fd, AIN1);
        int pot    = read_pcf8591(fd, AIN3);


        write_pcf8591(fd, AOUT, led_value);

        double temp_celsius = convert_to_celsius(temp);

        // 결과 출력 (0 ~ 255)
        printf(" 조도(LDR): %3d | 온도(Therm): %3f | 가변저항(Pot): %3d | LED(D1): %d\n", 
                ldr, temp_celsius, pot, led_value);

        if(led_value == 255){
            led_value = 0;
        }
        else{
            led_value = 255;
        }
        
        delay(500); // 0.5초 대기
    }

    return 0;
}
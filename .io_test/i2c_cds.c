#include <stdio.h>
#include <wiringPi.h> 
#include <wiringPiI2C.h>

int main (void)
{ 
    int fd; 
    int i, cnt; 
    int a2dChannel = 0; // analog channel AIN0, CDS sensor 
    int prev, a2dVal; 
    int threshold = 180; 
    printf("[ADC/DAC(YL-40) Module testing........]\n"); 
    /* 
    if((fd = wiringPiI2CSetup(0x48))<0) { 
        printf("wiringPiI2CSetup failed:\n"); 
    } 
    */ 
    if((fd = wiringPiI2CSetupInterface("/dev/i2c-1",0x48))<0) { 
        printf("wiringPiI2CSetupInterface failed:\n"); 
    }

    cnt = 0;

    while(1){ 
        wiringPiI2CWrite(fd, 0x00 | a2dChannel); // 0000_0000 
        prev = wiringPiI2CRead(fd); // Previously byte, garvage 
        a2dVal = wiringPiI2CRead(fd); 
        printf("[%d] prev = %d, ", cnt, prev); 
        printf("a2dVal = %d, ", a2dVal); 
        
        if(a2dVal < threshold) { 
            printf("Bright!!\n"); 
        } 
        else { 
            printf("Dark!!\n"); 
        } 
        delay(1000); 
        cnt++; 
    }
}
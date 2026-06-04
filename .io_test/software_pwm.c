#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <stdlib.h>

// analogpin gpio 6 -> 22
// pwm pin gpio 12,13 -> 26,23
void ledPwmControl(int gpio){
    pinMode(gpio, OUTPUT) ; 
    softPwmCreate(gpio, 0, 255);

    for(int i = 0; i < 10000; i++){
        softPwmWrite(gpio, i&255);
        delay(5);
    }

    softPwmWrite(gpio, 0);
}

int main(int argc, char** argv)
{
   int gno; 

   if(argc < 2) {
      printf("Usage : %s GPIO_NO\n", argv[0]);
      return -1;
   }
   gno = atoi(argv[1]);

   wiringPiSetup();                           /* wiringPi 초기화 */

   ledPwmControl(gno);

   return 0;
}
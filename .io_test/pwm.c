#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>

// analogpin gpio 6 -> 22
// pwm pin gpio 12,13 -> 26,23
void ledPwmControl(int gpio){
    if(gpio == 26 || gpio == 23 || gpio == 1){
        int bright ;

        pinMode(gpio, PWM_OUTPUT) ; 

        for (bright = 0 ; bright < 1024 ; ++bright)
        {
            pwmWrite (gpio, bright) ;
            delay (10) ;
        }

        for (bright = 1023 ; bright >= 0 ; --bright)
        {
            pwmWrite (gpio, bright) ;
            delay (10) ;
        }
    }
    else{
        printf("is not pwm in : %d\n", gpio);
        // return -1;
        exit(-1);
    }
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
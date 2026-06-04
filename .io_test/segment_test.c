#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>

int fndControl(int num){
    // int gpioins[4] = {0,2,3,4};         /* A, B, C, D : gpio 17, 27, 22, 23 */
    int gpioins[4] = {4,3,2,0};

    int number[10][4] ={
        {0,0,0,0},      // 0
        {0,0,0,1},      // 1
        {0,0,1,0},      // 2
        {0,0,1,1},      // 3
        {0,1,0,0},      // 4
        {0,1,0,1},      // 5
        {0,1,1,0},      // 6
        {0,1,1,1},      // 7
        {1,0,0,0},      // 8
        {1,0,0,1}       // 9
    };

    for(int i = 0; i < 4; i++){
        pinMode(gpioins[i], OUTPUT);
    }

    for(int i = 0; i < 4; i++){
        // digitalWrite(gpioins[i], number[num][i] ? LOW:HIGH);
        digitalWrite(gpioins[i], number[num][i] ? HIGH:LOW);
    }
    getchar();

    for(int i = 0; i < 4; i++){
        digitalWrite(gpioins[i], HIGH);
    }

    return 0;
}

int main(int argc, char **argv){
    int no;

    if(argc < 2){
        printf("Usage : %s NO\n", argv[0]);
        return -1;
    }

    no = atoi(argv[1]);
    wiringPiSetup();
    fndControl(no);

    return 0;
}
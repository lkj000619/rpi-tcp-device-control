TCP를 이용한 원격 장치 제어 프로그램

서버 - 라즈베리파이 4
클라이언트 - 우분투 리눅스

IO
- LED x3, pin GPIO 6/12/13 사용
- 부저 x1, pin GPIO 5 사용
- YL-40 PCF8591 PCB (조도센서) x1, pin GPIO 2/3 사용
- 7 세그먼트(애노드) x1 + 7447 IC x1, pin GPIO 17/27/22/23 사용

참고 사항 - LED pin GPIO 12/13은 PWM0/1 으로 기존 software PWM이 아닌 다른 함수를 사용합



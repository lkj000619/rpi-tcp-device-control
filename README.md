# TCP 원격 장치 제어 프로그램 (README)

라즈베리파이 4 기반의 임베디드 장비 제어 서버와 우분투 리눅스 환경의 Dumb Terminal 클라이언트 간 TCP 소켓 통신을 지원하는 원격 하드웨어 제어 솔루션입니다.

## 1. 하드웨어 구성 및 핀 매핑
원격 제어 서버가 탑재된 라즈베리파이 4 보드의 GPIO 매핑 설계는 다음과 같습니다.
- **3색 LED 제어**
  - 빨간색 LED: GPIO 6 (Soft PWM 지원)
  - 노란색 LED: GPIO 12 (Hard PWM - PWM0 지원)
  - 녹색 LED: GPIO 13 (Hard PWM - PWM1 지원)
- **가상 부저 제어**
  - 능동형/수동형 부저: GPIO 5 (softTone 이용 멜로디 재생)
- **7-Segment 디스플레이 (애노드형 + 74LS47 BCD 디코더 IC)**
  - A (LSB): GPIO 23
  - B: GPIO 22
  - C: GPIO 27
  - D (MSB): GPIO 17
  - *안정성 보강:* 74LS47의 3번(LT), 4번(BI/RBO), 5번(RBI) 핀은 VCC(5V)로 직접 풀업 연결하여 플로팅 상태에서의 오작동을 차단함.
- **YL-40 모듈 (PCF8591 I2C ADC/DAC 컨버터)**
  - SDA: GPIO 2
  - SCL: GPIO 3
  - *채널 명세:* Channel 0 (LDR 조도 센서), Channel 1 (NTC 서미스터 온도 센서), Channel 3 (가변저항 Potentiometer), Channel +64 (Analog OE - 보드 내장 D1 LED 출력)
## 2. 파일 및 디렉터리 구성

- [CMakeLists.txt](CMakeLists.txt): 프로젝트 전역 CMAKE 설정 파일
- [all_build.sh](all_build.sh): 메이크파일 생성 및 증분 빌드를 관장하는 통합 쉘 스크립트
- [deploy.sh](deploy.sh): 빌드 파일 변경 사항을 비교하여 라즈베리파이로 전송하는 스마트 배포 스크립트
- [include/io_control.h](include/io_control.h): 입출력 장치 제어 및 상태 공유 구조체 헤더
- [include/server_socket.h](include/server_socket.h): 소켓 통신 및 세션 관리 헤더
- [lib/CMakeLists.txt](lib/CMakeLists.txt): 원격 IO 라이브러리 빌드 설정
- [lib/io_control.c](lib/io_control.c): LED, 부저, 세그먼트 디바이스 비동기 스레드 구동 로직
- [lib/server_socket.c](lib/server_socket.c): epoll 멀티플렉싱 및 소켓 통신 상태 머신 핵심 로직
- [lib/yl40.c](lib/yl40.c): PCF8591 I2C 장치 데이터 읽기/쓰기 및 조도 오토 루프 구동
- [server/CMakeLists.txt](server/CMakeLists.txt): 서버 데몬 프로그램 빌드 설정
- [server/main.c](server/main.c): 서버 데몬 기동 및 동적 공유 라이브러리(`libRemoteIO.so`)의 `dlopen` 진입점
- [client/CMakeLists.txt](client/CMakeLists.txt): 클라이언트 프로그램 빌드 설정
- [client/main.c](client/main.c): 시그널 마스킹 처리 및 표준 입출력 중계 클라이언트 (Dumb Terminal)
- [docs/development_document.md](docs/development_document.md): 상세 개발 문서 (일정, 아키텍처 및 보완 조치)
## 3. 빌드 방법
### 빌드 요구사항
- CMake 3.10 이상
- GCC 컴파일러
- aarch64-linux-gnu-gcc 크로스컴파일러
- wiringPi 라이브러리 (서버 실행 대상 라즈베리파이 시스템 내 필수 설치)

<!-- ### 호스트 PC 크로스 툴체인 설치 (라즈베리파이 타겟 크로스 컴파일용)
호스트 PC(Ubuntu x86_64 등)에서 라즈베리파이 4 타겟용 ARM64 바이너리를 생성하려면 다음 패키지를 먼저 설치해야 합니다.
```bash
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
``` -->

### 전체 빌드 및 증분 빌드 수행 (all_build.sh)
전체 빌드 스크립트를 통해 클라이언트(x86_64)와 서버(ARM64 크로스 컴파일)를 순차적으로 빌드하여 `bin/` 폴더에 생성합니다.
```bash
# 전체 빌드 수행 (client: x86_64 네이티브, server: ARM64 크로스 컴파일)
./all_build.sh

# 전체 빌드 환경 정리 (Clean 빌드용)
./all_build.sh clean

# 전체 네이티브 빌드 수행 (Raspberry Pi에서 build 실행 시)
./all_build.sh native
```

### 개별 컴파일 수행
프로젝트의 독립성을 보장하기 위해 빌드 디렉터리(`build_client`, `build_server`)가 완전 격리되어 동작합니다.

#### 1) 클라이언트 단독 빌드 (client_build.sh)
클라이언트는 항상 호스트 x86_64 네이티브 바이너리로 컴파일됩니다.
```bash
# 클라이언트 빌드 기동
./client_build.sh

# 클라이언트 빌드 잔재 정리
./client_build.sh clean
```

#### 2) 서버 단독 빌드 (server_build.sh)
기본적으로 라즈베리파이 ARM64 크로스 컴파일을 수행하며, `native` 옵션으로 네이티브 환경 빌드도 가능합니다.
```bash
# 라즈베리파이 ARM64 크로스 컴파일 수행 (기본값)
./server_build.sh

# 호스트 네이티브 서버 빌드 수행 (Raspberry Pi에서 빌드 시 사용)
./server_build.sh native

# 서버 빌드 잔재 정리
./server_build.sh clean
```
## 4. 라즈베리파이 스마트 배포 (deploy.sh)

호스트 PC에서 컴파일된 라즈베리파이 서버 바이너리(`bin/server`) 및 공유 라이브러리(`bin/libRemoteIO.so`)의 변경 여부(MD5 해시)를 확인하여, 업데이트된 파일만 전송합니다.
```bash
# 변경된 빌드 산출물 전송 (사용자명과 IP 주소 입력)
./deploy.sh <pi_user> <ip_address>

# 예시
./deploy.sh ethan 172.20.26.241
```
- 인자가 누락되거나 잘못된 IP 주소(IPv4 규격 및 0~255 범위 검증)를 기입하면 오류 메시지를 띄우고 조기 종료합니다.
- 전송 전용 SSH 키 등록(비밀번호 없는 로그인)이 되어 있는 상태를 권장합니다.
- 변경 내역이 없는 파일은 전송을 생략합니다.

## 5. 실행 방법

### 서버 데몬 기동 (라즈베리파이 측)
서버 바이너리는 백그라운드 데몬으로 안전하게 분할되어 구동되며 시스템 로그(`syslog`)에 이벤트를 적재합니다.
```bash
# 서버 데몬 백그라운드 가동
./bin/server <log_filename>
```
### 클라이언트 접속 및 제어 (우분투 PC 측)
클라이언트는 서버 측의 IP 주소를 인자로 넘겨 접속을 시도합니다.
```bash
./bin/client <라즈베리파이_IP_주소>
```
- 접속이 성공하면 서버가 직접 렌더링하여 전송하는 CLI 메뉴 및 터미널 프롬프트에 따라 하드웨어를 제어할 수 있습니다.
- `Ctrl+C` 입력 시 소켓 연결이 안전하게 반환되며 종료됩니다.

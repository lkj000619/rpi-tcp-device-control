# TCP 원격 장치 제어 프로그램 (README)
라즈베리파이 4 기반의 임베디드 장비 제어 서버와 우분투 리눅스 환경의 Dumb Terminal 클라이언트 간 TCP 소켓 통신을 지원하는 원격 하드웨어 제어 솔루션입니다.
## 1. 하드웨어 구성 및 핀 매핑
---
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
---
- [CMakeLists.txt](file:///home/pang/project_test/CMakeLists.txt): 프로젝트 전역 CMAKE 설정 파일
- [build_all.sh](file:///home/pang/project_test/build_all.sh): 메이크파일 생성 및 증분 빌드를 관장하는 쉘 스크립트
- [include/common.h](file:///home/pang/project_test/include/common.h): 디바이스 핀 매핑 정보, 세션 상수, 프로토콜 헤더
- [lib/CMakeLists.txt](file:///home/pang/project_test/lib/CMakeLists.txt): 원격 IO 라이브러리 빌드 설정
- [lib/server.c](file:///home/pang/project_test/lib/server.c): TCP 소켓 분배, epoll 멀티플렉싱, 세션 처리 및 장치 구동 핵심 엔진
- [lib/yl40.c](file:///home/pang/project_test/lib/yl40.c): PCF8591 I2C 장치 데이터 읽기/쓰기 및 조도 오토 루프 구동
- [server/CMakeLists.txt](file:///home/pang/project_test/server/CMakeLists.txt): 서버 데몬 프로그램 빌드 설정
- [server/main.c](file:///home/pang/project_test/server/main.c): 서버 데몬 기동 및 동적 공유 라이브러리(`libRemoteIO.so`)의 `dlopen` 진입점
- [client/CMakeLists.txt](file:///home/pang/project_test/client/CMakeLists.txt): 클라이언트 프로그램 빌드 설정
- [client/main.c](file:///home/pang/project_test/client/main.c): 시그널 마스킹 처리 및 표준 입출력 중계 클라이언트 (Dumb Terminal)
- [docs/development_document.md](file:///home/pang/project_test/docs/development_document.md): 상세 개발 문서 (일정, 아키텍처 및 보완 조치)
## 3. 빌드 방법
---
### 빌드 요구사항
- CMake 3.10 이상
- GCC 컴파일러
- wiringPi 라이브러리 (라즈베리파이 시스템 내 필수 설치)
### 전체 빌드 및 증분 빌드 수행
제공되는 자동 빌드 스크립트를 사용하여 손쉽게 빌드할 수 있습니다.
```bash
# 빌드 수행 (변경된 소스코드만 선별하여 증분 빌드 수행)
./build_all.sh

# 기존 빌드 결과물 정리 (Clean 빌드용)
./build_all.sh clean
```
- 컴파일이 성공적으로 완료되면 최상위 디렉터리에 `bin/` 폴더가 생성되고 `bin/server` 및 `bin/client` 실행 파일이 자동 배치됩니다.
## 4. 실행 방법
---
### 서버 데몬 기동 (라즈베리파이 측)
서버 바이너리는 백그라운드 데몬으로 안전하게 분할되어 구동되며 시스템 로그(`syslog`)에 이벤트를 적재합니다.
```bash
# 서버 데몬 백그라운드 가동
./bin/server remoteIO_daemon
```
### 클라이언트 접속 및 제어 (우분투 PC 측)
클라이언트는 서버 측의 IP 주소를 인자로 넘겨 접속을 시도합니다.
```bash
./bin/client <라즈베리파이_IP_주소>
```
- 접속이 성공하면 서버가 직접 렌더링하여 전송하는 CLI 메뉴 및 터미널 프롬프트에 따라 하드웨어를 제어할 수 있습니다.
- `Ctrl+C` 입력 시 소켓 연결이 안전하게 반환되며 종료됩니다.

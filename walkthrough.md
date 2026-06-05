# 하드웨어 제어 원격 서버 구조화 리팩토링 완료 보고서
프로젝트 레이아웃의 가독성과 모듈성을 향상하기 위해 C/C++ 표준 디렉터리 구성 방식(include, lib)으로 코드를 정비하고, 변경된 파일만 영리하게 추적하여 증분 빌드(Incremental Build)를 지원하도록 최적화하는 리팩토링 작업을 성공적으로 완료했습니다.
## 리팩토링 후 디렉터리 구조
---
```text
rpi-tcp-device-control/
├── CMakeLists.txt (루트 설정)
├── all_build.sh (통합 빌드 제어 스크립트)
├── server_build.sh (서버 및 라이브러리 빌드 스크립트)
├── client_build.sh (클라이언트 빌드 스크립트)
├── toolchain_arm64.cmake (ARM64 크로스 컴파일 툴체인 설정)
├── scp.sh (라즈베리파이 파일 전송 스크립트)
├── deploy.sh (라즈베리파이 스마트 배포 스크립트)
├── walkthrough.md (이 완료 보고서)
├── README.md (프로젝트 사용 안내서)
├── analysis_results.md (프로젝트 아키텍처 및 개선 제안서)
├── docs/
│   ├── 2026_VEDA07_심화실습평가2(리눅스프로그래밍).md (평가 가이드)
│   └── development_document.md (세부 개발 명세서)
├── include/
│   ├── io_control.h (입출력 장치 제어 및 구조체 헤더)
│   └── server_socket.h (소켓 통신 및 세션 관리 헤더)
├── lib/
│   ├── CMakeLists.txt (공유 라이브러리 빌드)
│   ├── io_control.c (하드웨어 제어 및 스레드 캡슐화)
│   ├── server_socket.c (소켓 바인딩 및 세션 입력 분석)
│   └── yl40.c (I2C 센서 폴링 및 가로등 오토 제어)
├── server/
│   ├── CMakeLists.txt (데몬 실행 파일 빌드)
│   └── main.c (데몬 기동 및 라이브러리 로더)
└── client/
    ├── CMakeLists.txt (클라이언트 실행 파일 빌드)
    └── main.c (시그널 안전 텔넷 터미널)
```
## 상세 변경 및 이관 사항
---
### 1. include 폴더 신설 및 헤더 이관 ([include/common.h](file:///home/pang/project_test/include/common.h))
- 기존 `server/common.h` 파일을 프로젝트 전체가 안전하게 임포트할 수 있도록 루트의 `include/` 폴더 하위로 완전히 이관했습니다.
- LED/부저/FND용 하드웨어 핀 및 PCF8591 I2C 주소/채널 매핑 데이터, 세션 머신용 상태 변수들의 명세를 한 곳에서 일괄 집중 관리합니다.
### 2. lib 폴더 신설 및 디바이스 통신 로직 모듈화 ([lib/](file:///home/pang/project_test/lib/))
- 기존 `server/` 디렉터리 아래에 데몬 프로세스 코드와 섞여 있던 디바이스 구동 소스들(`server.c`, `yl40.c`)을 `lib/` 폴더 하위로 이동하여 하드웨어 제어 동적 라이브러리 개발 영역으로 명확히 구획했습니다.
- 라이브러리의 자체 컴파일을 통제하는 전용 [lib/CMakeLists.txt](file:///home/pang/project_test/lib/CMakeLists.txt)를 새로 만들어, 수학 모듈(`m`), 스레드 모듈(`pthread`), 하드웨어 제어 모듈(`wiringPi`)을 단독 타겟 링크 및 빌드하는 최적 규칙을 수립했습니다.
### 3. CMakeLists 빌드 구성 수정 및 이전 파일 클리닝
- 루트 [CMakeLists.txt](file:///home/pang/project_test/CMakeLists.txt)에 `include_directories(include)`를 전역 등록하여 소스 파일 간에 헤더 상대 경로 연동 에러를 근절했습니다.
- `add_subdirectory(lib)`를 명시하고 기존 [server/CMakeLists.txt](file:///home/pang/project_test/server/CMakeLists.txt)에서 복잡했던 라이브러리 빌드 내용을 걷어내어, 서버 바이너리 단독 빌드 체제로 깔끔하게 개편했습니다.
- 이관이 완료되어 잉여 상태가 된 기존 [server/common.h](file:///home/pang/project_test/server/common.h), [server/server.c](file:///home/pang/project_test/server/server.c), [server/yl40.c](file:///home/pang/project_test/server/yl40.c) 3개 파일을 프로젝트 경로에서 안전하게 삭제하고 폴더 구성을 일치시켰습니다.
### 4. 증분 빌드(Incremental Build) 최적화 ([build_all.sh](file:///home/pang/project_test/build_all.sh))
- 매번 빌드할 때마다 CMake 캐시를 지우고 `cmake ..`을 강제 구동하여 컴파일 시간을 과소비하던 비효율적인 루틴을 개선했습니다.
- 빌드 임시 폴더(`build`)와 Makefile이 존재할 경우 `cmake` 실행은 건너뛰고 `make`만 호출하여 변경 및 업데이트된 코드 파일(`.c`, `.h`)만 선별하여 빠르게 증분 컴파일하고 링킹하도록 쉘 스크립트 흐름을 재설정했습니다.
### 5. 안전성 보강을 위한 C 코드 리팩토링 ([lib/server.c](file:///home/pang/project_test/lib/server.c))
- `lib/server.c` 내의 약 30여 개 `write(fd, "...", N)` 형태의 하드코딩 크기 상수들을 `strlen("...")` 형태로 전면 교체하여 한글 메시지 전송 시 문자 깨짐 현상을 원천 방지하고 잠재적 버퍼 초과 정보 유출 가능성을 봉쇄했습니다.
- `led_blink`, `led_fade`, `yl40_d1_blink`, `seg_thread` 등 비동기로 백그라운드 동작하는 스레드들에 대해, 전달된 힙 동적 할당 메모리(`malloc`) 인자를 지역 변수로 복사한 뒤 루프 구동 전 즉시 `free()`를 수행하도록 설계했습니다. 이를 통해 비동기 스레드가 외부에서 강제 취소(`pthread_cancel`)되더라도 메모리 누수(Memory Leak)가 발생하지 않도록 자원 관리 안정성을 극대화했습니다.
### 6. 실시간 PWM 상태 가시성 및 값 연동 보완 ([include/common.h](file:///home/pang/project_test/include/common.h))
- [include/common.h](file:///home/pang/project_test/include/common.h) 내 `readIO_t` 공유 구조체에 실시간 3색 LED의 PWM 값(밝기)을 보관하는 `led_pwm_state` 필드를 추가했습니다.
- LED 조작(ON/OFF/밝기 상세 설정) 시 데이터가 스레드 안전하게 즉각 업데이트되도록 [lib/server.c](file:///home/pang/project_test/lib/server.c) 및 [lib/yl40.c](file:///home/pang/project_test/lib/yl40.c) 코드를 갱신했습니다.
- 클라이언트 세션의 LED 전용 상세 제어 메뉴와 PWM 설정 하위 메뉴 진입 시, 해당 LED의 실시간 밝기 상태가 메뉴 구성 텍스트(`현재 PWM: %d`)와 가이드 텍스트(`현재 PWM 밝기 값: %d`)에 직접 매핑되어 출력되도록 UI 흐름을 개선했습니다.
### 7. 백그라운드 데몬 파일 로깅 시스템 보완 ([lib/server.c](file:///home/pang/project_test/lib/server.c))
- 터미널 표준 스트림 차단 상태인 데몬 프로세스의 작동 상황을 감시하기 위해, 스레드 안전성(뮤텍스 락)을 보장하는 `write_daemon_log` 파일 로그 함수를 추가했습니다.
- 서버 구동 시점, 클라이언트의 접속 성공(접속 IP 주소 기록) 및 퇴장(단선 포함), 그리고 사용자가 송출한 액추에이터 제어 명령(LED ON/OFF/밝기설정값, 부저, 타이머 등), 조도 오토 모드에 의한 자동 핀 토글 등을 `remoteIO.log` 파일에 시간 정보([YYYY-MM-DD HH:MM:SS])와 함께 실시간 한글 기록하도록 통합하였습니다.
### 8. 라즈베리파이 ARM64 크로스 컴파일용 툴체인 연동 ([toolchain_arm64.cmake](file:///home/pang/project_test/toolchain_arm64.cmake))
- 호스트 PC(x86_64)에서 라즈베리파이 4 타겟 보드에 동작하는 ARM64 실행 파일을 크로스 빌드할 수 있도록 전용 툴체인 설정 파일인 [toolchain_arm64.cmake](file:///home/pang/project_test/toolchain_arm64.cmake)을 도입했습니다.
- [build_all.sh](file:///home/pang/project_test/build_all.sh)를 갱신하여 `./build_all.sh arm64` 커맨드 입력 시 CMake 빌드 캐시를 자동으로 깨끗하게 청소하고, 툴체인을 인계받아 에러 없이 한 번에 ARM64 바이너리를 빌드해 낼 수 있는 지능형 빌드 파이프라인을 구축했습니다.
## 라즈베리파이 실장비 검증 시나리오 및 절차
---
기기 이관 후 최종 레이아웃 빌드가 올바르게 완료되는지 다음의 지침에 따라 검증하시기 바랍니다.
### 1. 빌드 수행
```bash
./build_all.sh
```
- 최초 기동 시에는 CMake 메이크파일이 생성된 후 빌드가 진행되며, 두 번째부터는 `./build_all.sh`를 여러 번 반복 호출하더라도 업데이트된 소스파일만 빠르게 부분 컴파일(Incremental Compile)되는지 빌드 출력 속도를 확인합니다.

### 2. 스마트 배포 스크립트를 통한 라즈베리파이 전송
```bash
./deploy.sh <pi_user> <ip_address>
# 예시: ./deploy.sh ethan 172.20.26.241
```
- `deploy.sh` 실행 시 인자가 누락되거나 IP 형식이 올바르지 않으면 오류 메시지를 출력하고 종료합니다.
- 정상 인자 전달 시 빌드 산출물(`bin/server`, `bin/libRemoteIO.so`)의 MD5 해시를 비교하여 변경 사항(업데이트)이 있는 파일만 선별해서 `scp`로 라즈베리파이로 전송하는지 검증합니다.
- 변경된 파일이 없을 경우 `[알림] 변경된 파일이 없어 전송을 생략했습니다.` 문구가 출력되며 조기 종료되는지 교차 확인합니다.

### 3. 데몬 서버 구동 테스트
```bash
./bin/server remoteIO_daemon
```
- 데몬 프로세스가 백그라운드로 진입하면서, 동일 폴더 내의 `./libRemoteIO.so`를 찾아 epoll 소켓 및 센서 감시 루틴을 안정적으로 기동하는지 점검합니다.
### 3. 클라이언트 터미널 연결 및 상호작용 검증
```bash
./bin/client <라즈베리파이_IP>
```
- 서버가 송출해 주는 웰컴 텍스트와 프롬프트를 바탕으로 정상 통신 및 잠금(Lock) 처리가 이루어지는지 교차 검증합니다.
### 4. 안전성 및 누수 검증
- 클라이언트를 연결하여 동작시켰을 때 한글 웰컴 메시지나 에러 메시지가 도중에 잘리거나 깨지지 않고 올바르게 출력되는지 관찰합니다.
- `valgrind` 또는 `top`을 사용하여 서버 프로세스를 모니터링하며 LED BLINK, LED FADE, FND 카운트다운을 반복 실행하고 강제 취소시켰을 때 서버 메모리 사용량이 늘어나지 않고 릴리즈되는지 확인합니다.

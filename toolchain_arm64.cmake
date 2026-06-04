# CMake Toolchain File for Raspberry Pi ARM64 (aarch64) Cross Compilation

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 크로스 컴파일러 지정 (호스트 시스템에 gcc-aarch64-linux-gnu 패키지가 설치되어 있어야 함)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 타겟 시스템 루트(Sysroot) 설정
# (라즈베리파이 보드의 실제 라이브러리 및 wiringPi 헤더 복제본이 호스트에 존재할 경우 sysroot 경로를 바인딩함)
# set(CMAKE_SYSROOT /usr/arm-linux-gnueabihf)

# 크로스 빌드 시 검색 모드 설정
# 호스트 프로그램은 사용하되, 라이브러리 및 인클루드 헤더는 타겟 환경(sysroot)의 경로만 탐색하도록 제한
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

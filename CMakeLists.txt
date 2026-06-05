# CMake 최소 지원 버전 지정
cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름과 사용 언어 지정 
# (C++를 사용하신다면 C 대신 CXX를 입력하세요)
project(remote_IO C)

# 컴파일 옵션 추가 (선택 사항 - 경고 메시지 활성화)
add_compile_options(-Wall -Wextra)

# TCP 포트 번호 설정 (기본값: 60001)
set(PORT 60001 CACHE STRING "Server Port Number")
add_compile_definitions(PORT=${PORT})

# 공통 헤더 include 디렉토리 설정
include_directories(include)

# 빌드 결과물(바이너리 및 공유 라이브러리) 출력 폴더 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

# 빌드 대상 선택 옵션 제공 (기본값 ON)
option(BUILD_CLIENT "Build client application" ON)
option(BUILD_SERVER "Build server and remote IO library" ON)

# 만약 크로스 컴파일 중이라면 클라이언트 빌드는 강제로 비활성화
if(CMAKE_CROSSCOMPILING)
    set(BUILD_CLIENT OFF CACHE BOOL "Build client application" FORCE)
endif()

# 하위 디렉토리의 CMakeLists.txt를 실행하도록 지시
if(BUILD_SERVER)
    message(STATUS "서버 및 공유 라이브러리 빌드 활성화")
    add_subdirectory(lib)
    add_subdirectory(server)
endif()

if(BUILD_CLIENT)
    message(STATUS "클라이언트 어플리케이션 빌드 활성화")
    add_subdirectory(client)
endif()
# CMake 최소 지원 버전 지정
cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름과 사용 언어 지정 
# (C++를 사용하신다면 C 대신 CXX를 입력하세요)
project(remote_IO C)

# 컴파일 옵션 추가 (선택 사항 - 경고 메시지 활성화)
add_compile_options(-Wall -Wextra)

# 공통 헤더 include 디렉토리 설정
include_directories(include)

# 빌드 결과물(바이너리 및 공유 라이브러리) 출력 폴더 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

# 하위 디렉토리의 CMakeLists.txt를 실행하도록 지시
add_subdirectory(lib)
add_subdirectory(server)
add_subdirectory(client)
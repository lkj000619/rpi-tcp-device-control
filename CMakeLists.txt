# CMake 최소 지원 버전 지정
cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름과 사용 언어 지정 
# (C++를 사용하신다면 C 대신 CXX를 입력하세요)
project(remote_IOctrl C)

# 컴파일 옵션 추가 (선택 사항 - 경고 메시지 활성화)
add_compile_options(-Wall -Wextra)

# 하위 디렉토리의 CMakeLists.txt를 실행하도록 지시
add_subdirectory(server)
add_subdirectory(client)
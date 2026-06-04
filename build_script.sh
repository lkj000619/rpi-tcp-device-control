#!/bin/bash

# 1. 'clean' 옵션 처리: 기존 빌드 파일과 바이너리를 깔끔하게 삭제
if [ "$1" == "clean" ]; then
    echo "🧹 기존 빌드 폴더(build)와 실행 파일 폴더(bin)를 정리합니다..."
    rm -rf build bin
    echo "✨ 정리가 완료되었습니다."
    exit 0
fi

echo "🚀 프로젝트 빌드를 시작합니다..."

# 2. build 폴더가 없으면 새로 생성
if [ ! -d "build" ]; then
    mkdir build
fi

# 3. build 폴더로 이동하여 CMake 및 Make 실행
cd build
cmake ..
make

# 4. 빌드 성공 여부 확인 및 안내
if [ $? -eq 0 ]; then
    echo "✅ 빌드가 성공적으로 완료되었습니다!"
    echo "👉 실행 방법: ./bin/server 또는 ./bin/client <IP> <PORT>"
else
    echo "❌ 빌드 중 오류가 발생했습니다. 코드를 다시 확인해주세요."
fi
#!/bin/bash

# 1. 'clean' 옵션 처리
if [ "$1" == "clean" ]; then
    echo "🧹 서버 빌드 폴더(build_server)와 실행 파일/라이브러리(bin/server, bin/libRemoteIO.so)를 정리합니다..."
    rm -rf build_server
    if [ -f "bin/server" ]; then
        rm -f bin/server
    fi
    if [ -f "bin/libRemoteIO.so" ]; then
        rm -f bin/libRemoteIO.so
    fi
    echo "✨ 서버 정리가 완료되었습니다."
    exit 0
fi

CMAKE_FLAGS="-DBUILD_CLIENT=OFF -DBUILD_SERVER=ON"
BUILD_MODE="ARM64 크로스 컴파일"

# 2. native 옵션 분석 및 크로스 빌드 플래그 설정
if [ "$1" == "native" ]; then
    echo "🏗️  호스트 x86_64 네이티브 빌드로 서버를 컴파일합니다."
    BUILD_MODE="x86_64 네이티브"
else
    echo "🏗️  라즈베리파이 ARM64 크로스 컴파일로 서버를 컴파일합니다."
    # 크로스컴파일 툴체인 지정
    CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=../toolchain_arm64.cmake"
fi

echo "🚀 서버 빌드를 시작합니다 ($BUILD_MODE)..."

# 3. build_server 폴더 생성 및 이동
if [ ! -d "build_server" ]; then
    mkdir build_server
fi

cd build_server

# 4. CMake 및 Make 구동
echo "⚙️  CMake 메이크파일을 구성합니다..."
cmake $CMAKE_FLAGS ..

echo "🔨 서버 및 라이브러리를 증분 빌드합니다..."
make
make_status=$?

cd ..

# 5. 빌드 결과 확인
if [ $make_status -eq 0 ]; then
    echo "✅ 서버 빌드가 성공적으로 완료되었습니다! ($BUILD_MODE)"
    if [ "$BUILD_MODE" == "ARM64 크로스 컴파일" ]; then
        echo "👉 ARM64용 바이너리가 bin/ 디렉토리에 생성되었습니다. 라즈베리파이 보드로 전송해 실행하세요."
    else
        echo "👉 x86_64용 바이너리가 bin/ 디렉토리에 생성되었습니다."
    fi
else
    echo "❌ 서버 빌드 중 오류가 발생했습니다."
    exit 1
fi

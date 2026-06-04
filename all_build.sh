#!/bin/bash

# 에러 발생 시 즉시 중단되도록 설정
set -e

# 1. 'clean' 처리
if [ "$1" == "clean" ]; then
    echo "🧹 전체 빌드 환경을 정리합니다..."
    ./client_build.sh clean
    ./server_build.sh clean
    echo "✨ 전체 정리가 완료되었습니다."
    exit 0
fi

# 2. 빌드 실행 환경 분석
SERVER_MODE="arm64"
if [ "$1" == "native" ]; then
    SERVER_MODE="native"
fi

echo "🚀 통합 빌드를 시작합니다..."
echo "----------------------------------------"

# 3. 클라이언트 빌드 실행 (x86_64 네이티브 고정)
echo "📦 1단계: 클라이언트 빌드 구동"
./client_build.sh

echo "----------------------------------------"

# 4. 서버 빌드 실행 (옵션 전파: arm64 또는 native)
echo "📦 2단계: 서버 빌드 구동 ($SERVER_MODE)"
./server_build.sh $SERVER_MODE

echo "----------------------------------------"
echo "✅ 통합 빌드 프로세스가 모두 성공적으로 종료되었습니다!"

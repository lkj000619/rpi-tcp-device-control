#!/bin/bash

# 1. 'clean' 옵션 처리
if [ "$1" == "clean" ]; then
    echo "🧹 클라이언트 빌드 폴더(build_client)와 실행 파일(bin/client)을 정리합니다..."
    rm -rf build_client
    if [ -f "bin/client" ]; then
        rm -f bin/client
    fi
    echo "✨ 클라이언트 정리가 완료되었습니다."
    exit 0
fi

echo "🚀 클라이언트 빌드를 시작합니다 (x86_64 네이티브)..."

# 2. build_client 폴더 생성 및 이동
if [ ! -d "build_client" ]; then
    mkdir build_client
fi

cd build_client

# 3. CMake 구동 (클라이언트만 빌드하도록 캐시 옵션 설정)
echo "⚙️  CMake 메이크파일을 구성합니다..."
cmake -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF ..

# 4. 컴파일 진행
echo "🔨 클라이언트를 증분 빌드합니다..."
make
make_status=$?

cd ..

# 5. 빌드 결과 확인
if [ $make_status -eq 0 ]; then
    echo "✅ 클라이언트 빌드가 성공적으로 완료되었습니다! (bin/client)"
else
    echo "❌ 클라이언트 빌드 중 오류가 발생했습니다."
    exit 1
fi

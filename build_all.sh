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

# 3. build 폴더 내부로 이동
cd build

# 4. Makefile이 존재하지 않는 최초 기동 시에만 cmake 구동
# (Makefile이 이미 있는 상태에서 CMakeLists.txt가 갱신되면 make가 알아서 내부적으로 cmake를 다시 호출합니다.)
if [ ! -f "Makefile" ]; then
    echo "⚙️ 최초 빌드를 위해 CMake 메이크파일을 생성합니다..."
    cmake ..
fi

# 5. Make 실행 (변경된 소스 파일만 선택적으로 컴파일 및 링킹 수행)
echo "🔨 업데이트된 소스코드 파일만 새롭게 증분 빌드합니다..."
make
make_status=$?

# 6. 상위 프로젝트 루트 디렉터리로 복귀
cd ..

# 7. 빌드 성공 여부 확인 및 안내
if [ $make_status -eq 0 ]; then
    echo "✅ 빌드가 성공적으로 완료되었습니다!"
    echo "👉 실행 방법: ./bin/server 또는 ./bin/client <IP>"
else
    echo "❌ 빌드 중 오류가 발생했습니다. 코드를 다시 확인해주세요."
    exit 1
fi
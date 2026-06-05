#!/bin/bash

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# [A안 - 인자 개수 검사 및 에러 처리 (기본 적용)]
if [ $# -lt 2 ]; then
    echo -e "${RED}[오류] 입력 인자가 부족합니다.${NC}"
    echo -e "${YELLOW}사용법: $0 <pi_user> <ip_address>${NC}"
    echo -e "예시: $0 ethan 172.20.26.241"
    exit 1
fi
PI_USER="$1"
IP="$2"

# 기본값 Fallback 설정
# if [ $# -lt 2 ]; then
#     PI_USER="ethan"
#     IP="172.20.26.241"
#     echo -e "${YELLOW}[안내] 입력 인자가 부족하여 기본 설정을 적용합니다 (${PI_USER}@${IP})${NC}"
# else
#     PI_USER="$1"
#     IP="$2"
# fi

# IP 주소 IPv4 유효성 및 옥텟(0~255) 값 검증
if [[ ! $IP =~ ^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$ ]]; then
    echo -e "${RED}[오류] 잘못된 IP 주소 형식입니다: '$IP' (IPv4 형식을 입력해 주세요)${NC}"
    exit 1
fi

for i in 1 2 3 4; do
    if (( ${BASH_REMATCH[i]} < 0 || ${BASH_REMATCH[i]} > 255 )); then
        echo -e "${RED}[오류] IP 주소 범위가 올바르지 않습니다: '$IP' (각 옥텟은 0~255 사이여야 합니다)${NC}"
        exit 1
    fi
done

PI_DEST_DIR="~/"

# 배포 관리 파일
META_FILE=".last_deploy_hash"
TARGET_FILES=("bin/server" "bin/libRemoteIO.so")

echo -e "${BLUE}============ [스마트 배포 시스템 기동] ============${NC}"

# 전송할 파일의 실재 여부 체크
for FILE in "${TARGET_FILES[@]}"; do
    if [ ! -f "$FILE" ]; then
        echo -e "${RED}[오류] 배포할 파일 '$FILE'이 존재하지 않습니다. 빌드를 먼저 수행하세요.${NC}"
        exit 1
    fi
done

# 메타 파일이 없으면 초기화
if [ ! -f "$META_FILE" ]; then
    touch "$META_FILE"
fi

# 임시 메타 파일 생성용
TEMP_META=$(mktemp)

ANY_UPDATED=0

for FILE in "${TARGET_FILES[@]}"; do
    # 현재 파일의 MD5 해시값 계산
    CURRENT_HASH=$(md5sum "$FILE" | awk '{print $1}')
    FILE_NAME=$(basename "$FILE")
    
    # 이전 저장된 해시값 로드
    PREV_HASH=$(grep "^$FILE_NAME:" "$META_FILE" | cut -d':' -f2)
    
    if [ "$CURRENT_HASH" != "$PREV_HASH" ]; then
        echo -e "${YELLOW}[감지] '$FILE_NAME' 파일이 업데이트되었습니다. 전송을 시작합니다...${NC}"
        
        # scp 전송 실행
        scp "$FILE" "${PI_USER}@${IP}:${PI_DEST_DIR}"
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}[성공] '$FILE_NAME' 전송 완료.${NC}"
            ANY_UPDATED=1
        else
            echo -e "${RED}[오류] '$FILE_NAME' 전송 실패! 네트워크 또는 연결 설정을 확인하세요.${NC}"
            # 실패 시 이전 해시값을 그대로 유지하여 다음 실행 시 재전송되도록 처리
            CURRENT_HASH="$PREV_HASH"
        fi
    else
        echo -e "[유지] '$FILE_NAME' 파일에 변경 사항이 없습니다."
    fi
    
    # 임시 메타 파일에 새 해시 기록
    if [ -n "$CURRENT_HASH" ]; then
        echo "$FILE_NAME:$CURRENT_HASH" >> "$TEMP_META"
    fi
done

# 메타 파일 교체
mv "$TEMP_META" "$META_FILE"

if [ $ANY_UPDATED -eq 0 ]; then
    echo -e "${GREEN}[알림] 변경된 파일이 없어 전송을 생략했습니다.${NC}"
else
    echo -e "${GREEN}[알림] 배포가 성공적으로 완료되었습니다.${NC}"
fi

echo -e "${BLUE}===================================================${NC}"

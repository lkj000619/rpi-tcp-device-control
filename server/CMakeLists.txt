# 메인 서버 데몬 실행 파일 생성
add_executable(server main.c)
target_link_libraries(server dl pthread)
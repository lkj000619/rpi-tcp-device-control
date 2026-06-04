# 1. 동적 공유 라이브러리 (libRemoteIO.so) 생성
add_library(RemoteIO SHARED yl40.c server.c)
target_link_libraries(RemoteIO wiringPi m pthread)
set_target_properties(RemoteIO PROPERTIES OUTPUT_NAME "RemoteIO")

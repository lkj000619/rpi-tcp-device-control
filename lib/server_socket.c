#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <wiringPi.h>
#include <softPwm.h>

#include "io_control.h"
#include "server_socket.h"

static const char* led_color_names[] = {"", "빨간색 LED", "노란색 LED", "녹색 LED"};
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static client_session_t client_sessions[MAX_CLIENTS];
static char daemon_log_filename[256] = "remoteIO.log";

void set_daemon_log_filename(const char* filename) {
    pthread_mutex_lock(&log_mutex);
    if (filename && strlen(filename) > 0) {
        strncpy(daemon_log_filename, filename, sizeof(daemon_log_filename) - 1);
    }
    pthread_mutex_unlock(&log_mutex);
}

void write_daemon_log(const char* level, const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen(daemon_log_filename, "a");
    if (fp != NULL) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char time_buf[26];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        
        fprintf(fp, "[%s] [%s] ", time_buf, level);
        
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        
        fflush(fp);
        fclose(fp);
    }
    pthread_mutex_unlock(&log_mutex);
}

void send_menu_and_prompt(int fd, client_session_t *session, readIO_t *readIO) {
    char menu_buf[1024] = {0};
    char prompt_buf[128] = {0};
    char total_buf[2048] = {0};
    int current_session = session->current_session;
    int selected_led = session->selected_led;

    switch (current_session) {
        case SESSION_MAIN:
            sprintf(menu_buf, 
                "\n========== [ 메인 메뉴 ] ==========\n"
                " 1. LED 제어\n"
                " 2. 부저 제어\n"
                " 3. YL-40 모듈 제어\n"
                " 4. 7-Segment 타이머 제어\n"
                "-1. 프로그램 종료\n"
                "-----------------------------------\n"
                " 접속할 세션 번호를 입력하세요:\n");
            sprintf(prompt_buf, "[ MAIN ] > ");
            break;
            
        case SESSION_LED_SELECT:
            sprintf(menu_buf,
                "\n---------- [ LED 대상 선택 ] ----------\n"
                " [1] 빨간색 LED (Soft PWM)\n"
                " [2] 노란색 LED (Hard PWM)\n"
                " [3] 녹색 LED   (Hard PWM)\n"
                " --------------------------------------\n"
                " 제어할 LED 번호를 선택하세요 (1~3):\n"
                " (-1. 이전 단계)\n");
            sprintf(prompt_buf, "[ LED ] > ");
            break;

        case SESSION_LED_ACTION: {
            pthread_mutex_lock(readIO->mutex);
            int cur_pwm = readIO->led_pwm_preset[selected_led];
            pthread_mutex_unlock(readIO->mutex);
            sprintf(menu_buf,
                "\n------- [ %s 전용 제어 ] -------\n"
                "  1. 단순 제어 : ON/OFF 설정 세션 진입\n"
                "  2. 밝기 조절 : PWM 상세 조절 세션 진입 (현재 PWM: %d)\n"
                "  3. BLINK ( 깜빡임 )\n"
                "  4. FADE ( 숨쉬기 효과 )\n"
                " ------------------------------------------\n"
                " 메뉴 번호를 입력하세요:\n"
                " (-1. 이전 단계)\n", led_color_names[selected_led], cur_pwm);
            if (selected_led == 1) sprintf(prompt_buf, "[ LED:빨간 ] > ");
            else if (selected_led == 2) sprintf(prompt_buf, "[ LED:노란 ] > ");
            else if (selected_led == 3) sprintf(prompt_buf, "[ LED:녹색 ] > ");
            break;
        }

        case SESSION_LED_ONOFF:
            sprintf(menu_buf,
                "\n------- [ %s ON/OFF ] -------\n"
                " - 입력형식 : <ON 1 / OFF 0>\n"
                " - 예시 : 1 (켜기) 또는 0 (끄기)\n"
                " ------------------------------------------\n"
                " 명령어 입력:\n"
                " (-1. 이전 단계)\n", led_color_names[selected_led]);
            if (selected_led == 1) sprintf(prompt_buf, "[ 빨간(ON/OFF) ] > ");
            else if (selected_led == 2) sprintf(prompt_buf, "[ 노란(ON/OFF) ] > ");
            else if (selected_led == 3) sprintf(prompt_buf, "[ 녹색(ON/OFF) ] > ");
            break;

        case SESSION_LED_PWM: {
            pthread_mutex_lock(readIO->mutex);
            int cur_pwm = readIO->led_pwm_preset[selected_led];
            pthread_mutex_unlock(readIO->mutex);
            sprintf(menu_buf,
                "\n------- [ %s 밝기(PWM) 상세 조절 ] -------\n"
                " - [!] 현재 PWM 밝기 값: %d\n"
                " - 0 부터 255 사이의 값을 입력하세요.\n"
                " - (255 = 최대 밝기 / 128 = 중간 밝기 / 0 = 꺼짐)\n"
                " ------------------------------------------\n"
                " 원하는 밝기 숫자를 입력하세요:\n"
                " (-1. 이전 단계)\n", led_color_names[selected_led], cur_pwm);
            if (selected_led == 1) sprintf(prompt_buf, "[ 빨간(PWM) ] > ");
            else if (selected_led == 2) sprintf(prompt_buf, "[ 노란(PWM) ] > ");
            else if (selected_led == 3) sprintf(prompt_buf, "[ 녹색(PWM) ] > ");
            break;
        }

        case SESSION_BUZ:
            sprintf(menu_buf,
                "\n--------- [ 부저 세션 ] ---------\n"
                " - 입력형식 : <ON 1 / OFF 0>\n"
                " - 예시 : 1 (켜기) 또는 0 (끄기)\n"
                " ---------------------------------\n"
                " 명령어 입력:\n"
                " (-1. 이전 단계)\n");
            sprintf(prompt_buf, "[ BUZ ] > ");
            break;
            
        case SESSION_YL40:
            sprintf(menu_buf,
                "\n-------- [ YL-40 세션 ] --------\n"
                "  1. 센서 읽기 (READ)\n"
                "  2. 오토 모드 제어 세션 진입\n"
                "  3. 오토 모드 기준값(Threshold) 설정 세션 진입\n"
                "  4. D1 LED 상세 제어 세션 진입\n"
                " --------------------------------\n"
                " 메뉴 번호를 입력하세요:\n"
                " (-1. 이전 단계)\n");
            sprintf(prompt_buf, "[ YL-40 ] > ");
            break;

        case SESSION_YL40_AUTO:
            pthread_mutex_lock(readIO->mutex);
            int cur_auto = readIO->auto_mode;
            pthread_mutex_unlock(readIO->mutex);
            sprintf(menu_buf,
                "\n------- [ 오토 모드 ON/OFF 제어 ] -------\n"
                " - [!] 현재 오토 모드 상태: %s\n"
                " - 입력형식 : <ON 1 / OFF 0>\n"
                " - 예시 : 1 (켜기) 또는 0 (끄기)\n"
                " ----------------------------------------\n"
                " 변경할 상태를 입력하세요:\n"
                " (-1. 이전 단계)\n", cur_auto ? "ON" : "OFF");
            sprintf(prompt_buf, "[ YL-40:AUTO설정 ] > ");
            break;

        case SESSION_YL40_THRESHOLD:
            pthread_mutex_lock(readIO->mutex);
            int cur_thresh = readIO->threshold;
            pthread_mutex_unlock(readIO->mutex);
            sprintf(menu_buf,
                "\n---- [ 오토 모드 기준값(Threshold) 설정 ] ----\n"
                " - [!] 현재 기준값: %d\n"
                " - 어두움을 판단할 조도 센서 기준값을 입력하세요 (0~255)\n"
                " - 값이 클수록 더 어두워져야 LED가 켜집니다.\n"
                " ----------------------------------------------\n"
                " 설정할 숫자(기준값)를 입력하세요:\n"
                " (-1. 이전 단계)\n", cur_thresh);
            sprintf(prompt_buf, "[ YL-40:기준값설정 ] > ");
            break;

        case SESSION_YL40_D1: {
            pthread_mutex_lock(readIO->mutex);
            int cur_pwm = readIO->d1_led_preset;
            pthread_mutex_unlock(readIO->mutex);
            sprintf(menu_buf,
                "\n------- [ YL-40 D1 LED 상세 제어 ] -------\n"
                "  1. 단순 제어 : D1 LED ON/OFF 세션 진입\n"
                "  2. 밝기 조절 : PWM 상세 조절 세션 진입 (현재 PWM: %d)\n"
                "  3. 특수 효과 : BLINK (깜빡임)\n"
                " ------------------------------------------\n"
                " 메뉴 번호를 입력하세요:\n"
                " (-1. 이전 단계)\n", cur_pwm);
            sprintf(prompt_buf, "[ YL-40:D1 ] > ");
            break;
        }
        
        case SESSION_YL40_D1_ONOFF:
            sprintf(menu_buf,
                "\n------- [ YL-40 D1 LED ON/OFF ] -------\n"
                " - 입력형식 : <ON 1 / OFF 0>\n"
                " - 예시 : 1 (켜기) 또는 0 (끄기)\n"
                " ------------------------------------------\n"
                " 명령어 입력:\n"
                " (-1. 이전 단계)\n");
            sprintf(prompt_buf, "[ YL-40:D1(ON/OFF) ] > ");
            break;

        case SESSION_YL40_D1_PWM: {
            pthread_mutex_lock(readIO->mutex);
            int cur_pwm = readIO->d1_led_preset;
            pthread_mutex_unlock(readIO->mutex);
            sprintf(menu_buf,
                "\n------- [ YL-40 D1 밝기(PWM) 상세 조절 ] -------\n"
                " - [!] 현재 PWM 밝기 값: %d\n"
                " - 0 부터 255 사이의 값을 입력하세요.\n"
                " - (255 = 최대 밝기 / 128 = 중간 밝기 / 0 = 꺼짐)\n"
                " ------------------------------------------\n"
                " 원하는 밝기 숫자를 입력하세요:\n"
                " (-1. 이전 단계)\n", cur_pwm);
            sprintf(prompt_buf, "[ YL-40:D1(PWM) ] > ");
            break;
        }
            
        case SESSION_SEG:
            sprintf(menu_buf,
                "\n------ [ 7-Segment 세션 ] ------\n"
                " 입력형식: <0~9> (초 단위 카운트다운)\n"
                " 예시: 5\n"
                " (-1. 이전 단계)\n");
            sprintf(prompt_buf, "[ SEG ] > ");
            break;
    }

    sprintf(total_buf, "%s%s", menu_buf, prompt_buf);
    write(fd, total_buf, strlen(total_buf));
}

int process_client_input(client_session_t *session, char *input, readIO_t *readIO) {
    int fd = session->fd;
    input[strcspn(input, "\r\n")] = 0;
    
    while(*input == ' ') input++;
    int in_len = strlen(input);
    while(in_len > 0 && input[in_len - 1] == ' ') {
        input[in_len - 1] = '\0';
        in_len--;
    }

    if (strlen(input) == 0) {
        send_menu_and_prompt(fd, session, readIO);
        return 1;
    }

    if (strcmp(input, "-1") == 0 || strcmp(input, "exit") == 0) {
        if (session->current_session == SESSION_MAIN) {
            char* exit_msg = "서버: 클라이언트를 종료합니다.\n";
            write(fd, exit_msg, strlen(exit_msg));
            return 0;
        }
        
        int cur = session->current_session;
        if (cur == SESSION_LED_PWM || cur == SESSION_LED_ONOFF) {
            session->current_session = SESSION_LED_ACTION;
        } 
        else if (cur == SESSION_LED_ACTION) {
            int led_num = session->selected_led;
            stop_led_thread(led_num);
            int target_pin = (led_num == 1) ? LED1_PIN : ((led_num == 2) ? LED2_PIN : LED3_PIN);
            if (led_num == 1) softPwmWrite(target_pin, 0);
            else pwmWrite(target_pin, 0);
            
            pthread_mutex_lock(readIO->mutex);
            readIO->led_pwm_state[led_num] = 0;
            pthread_mutex_unlock(readIO->mutex);

            session->current_session = SESSION_LED_SELECT;
        } 
        else if (cur == SESSION_LED_SELECT) {
            session->current_session = SESSION_MAIN;
        }
        else if (cur == SESSION_BUZ) {
            stop_buz_thread();
            session->current_session = SESSION_MAIN;
        }
        else if (cur == SESSION_YL40) {
            session->current_session = SESSION_MAIN;
        }
        else if (cur == SESSION_SEG) {
            stop_seg_thread();
            session->current_session = SESSION_MAIN;
        }
        else if (cur == SESSION_YL40_THRESHOLD) {
            session->current_session = SESSION_YL40;
        }
        else if (cur == SESSION_YL40_AUTO) {
            pthread_mutex_lock(readIO->mutex);
            readIO->auto_mode = 0;
            readIO->led_pwm_state[1] = 0;
            readIO->led_pwm_state[2] = 0;
            readIO->led_pwm_state[3] = 0;
            pthread_mutex_unlock(readIO->mutex);

            stop_led_thread(1);
            stop_led_thread(2);
            stop_led_thread(3);

            softPwmWrite(LED1_PIN, 0);
            pwmWrite(LED2_PIN, 0);
            pwmWrite(LED3_PIN, 0);

            session->current_session = SESSION_YL40;
        }
        else if (cur == SESSION_YL40_D1) {
            stop_led_thread(0);
            pthread_mutex_lock(readIO->mutex);
            readIO->d1_led_state = 0;
            pthread_mutex_unlock(readIO->mutex);
            write_pcf8591(readIO->yl40_fd, YL40_AOUT, 0);

            session->current_session = SESSION_YL40;
        } 
        else if (cur == SESSION_YL40_D1_PWM || cur == SESSION_YL40_D1_ONOFF) {
            session->current_session = SESSION_YL40_D1;
        }
        send_menu_and_prompt(fd, session, readIO);
        return 1;
    }

    char response[256] = {0};
    int current = session->current_session;

    #define CHECK_LOCK() \
    pthread_mutex_lock(readIO->mutex); \
    if (readIO->active_client_fd == -1) { \
        readIO->active_client_fd = fd; \
    } else if (readIO->active_client_fd != fd) { \
        pthread_mutex_unlock(readIO->mutex); \
        char* lock_err = "서버: [에러] 다른 사용자가 출력 장치 제어를 점유 중입니다.\n"; \
        write(fd, lock_err, strlen(lock_err)); \
        send_menu_and_prompt(fd, session, readIO); \
        return 1; \
    } \
    pthread_mutex_unlock(readIO->mutex);

    switch (current) {
        case SESSION_MAIN:
            if (strcmp(input, "1") == 0) session->current_session = SESSION_LED_SELECT;
            else if (strcmp(input, "2") == 0) session->current_session = SESSION_BUZ;
            else if (strcmp(input, "3") == 0) session->current_session = SESSION_YL40;
            else if (strcmp(input, "4") == 0) session->current_session = SESSION_SEG;
            else {
                char* invalid_menu = "서버: 잘못된 메뉴 번호입니다.\n";
                write(fd, invalid_menu, strlen(invalid_menu));
            }
            break;

        case SESSION_LED_SELECT: {
            int num = atoi(input);
            if (num >= 1 && num <= 3) {
                session->selected_led = num;
                session->current_session = SESSION_LED_ACTION;
            } else {
                char* invalid_led = "서버: 잘못된 LED 선택입니다. (1~3 범위 입력)\n";
                write(fd, invalid_led, strlen(invalid_led));
            }
            break;
        }

        case SESSION_LED_ACTION: {
            int led_num = session->selected_led;
            int target_pin = (led_num == 1) ? LED1_PIN : ((led_num == 2) ? LED2_PIN : LED3_PIN);
            int is_soft = (led_num == 1);
            
            if (strcmp(input, "1") == 0) {
                session->current_session = SESSION_LED_ONOFF;
            }
            else if (strcmp(input, "2") == 0) {
                session->current_session = SESSION_LED_PWM;
            }
            else if (strcmp(input, "3") == 0) {
                CHECK_LOCK();
                start_led_blink(led_num, target_pin, is_soft);
                sprintf(response, "서버: %s BLINK 효과를 구동합니다.\n", led_color_names[led_num]);
                write(fd, response, strlen(response));
            }
            else if (strcmp(input, "4") == 0) {
                CHECK_LOCK();
                start_led_fade(led_num, target_pin, is_soft);
                sprintf(response, "서버: %s FADE 효과를 구동합니다.\n", led_color_names[led_num]);
                write(fd, response, strlen(response));
            }
            else {
                char* invalid_opt = "서버: 잘못된 동작 옵션입니다.\n";
                write(fd, invalid_opt, strlen(invalid_opt));
            }
            break;
        }

        case SESSION_LED_ONOFF: {
            CHECK_LOCK();
            int led_num = session->selected_led;
            int target_pin = (led_num == 1) ? LED1_PIN : ((led_num == 2) ? LED2_PIN : LED3_PIN);
            int is_soft = (led_num == 1);
            stop_led_thread(led_num);

            if (strcmp(input, "1") == 0 || strcasecmp(input, "on") == 0) {
                pthread_mutex_lock(readIO->mutex);
                int val = readIO->led_pwm_preset[led_num];
                pthread_mutex_unlock(readIO->mutex);

                if (is_soft) softPwmWrite(target_pin, val);
                else pwmWrite(target_pin, (val * 1023) / 255);

                pthread_mutex_lock(readIO->mutex);
                readIO->led_pwm_state[led_num] = val;
                pthread_mutex_unlock(readIO->mutex);
                sprintf(response, "서버: %s를 점등(ON)했습니다 (밝기: %d).\n", led_color_names[led_num], val);
                write(fd, response, strlen(response));
            }
            else if (strcmp(input, "0") == 0 || strcasecmp(input, "off") == 0) {
                if (is_soft) softPwmWrite(target_pin, 0);
                else pwmWrite(target_pin, 0);
                pthread_mutex_lock(readIO->mutex);
                readIO->led_pwm_state[led_num] = 0;
                pthread_mutex_unlock(readIO->mutex);
                sprintf(response, "서버: %s를 소등(OFF)했습니다.\n", led_color_names[led_num]);
                write(fd, response, strlen(response));
            }
            else {
                char* invalid_onoff = "서버: 잘못된 값입니다. (1 또는 0 입력)\n";
                write(fd, invalid_onoff, strlen(invalid_onoff));
            }
            break;
        }

        case SESSION_LED_PWM: {
            CHECK_LOCK();
            int val = atoi(input);
            if (val >= 0 && val <= 255) {
                int led_num = session->selected_led;
                int target_pin = (led_num == 1) ? LED1_PIN : ((led_num == 2) ? LED2_PIN : LED3_PIN);
                int is_soft = (led_num == 1);
                stop_led_thread(led_num);

                pthread_mutex_lock(readIO->mutex);
                readIO->led_pwm_preset[led_num] = val; // 프리셋 무조건 변경 및 보존
                int is_on = (readIO->led_pwm_state[led_num] > 0); // 현재 물리 핀 점등 여부

                if (is_on) {
                    // 현재 켜져 있을 때만 바뀐 밝기를 실시간 핀 제어에 반영
                    if (is_soft) softPwmWrite(target_pin, val);
                    else pwmWrite(target_pin, (val * 1023) / 255);
                    readIO->led_pwm_state[led_num] = val;
                }
                pthread_mutex_unlock(readIO->mutex);
                
                write_daemon_log("INFO", "[Socket: %d] LED 제어: %s 설정 밝기 변경 (밝기값: %d, 실시간 반영 여부: %s)\n", 
                                 fd, led_color_names[led_num], val, is_on ? "YES" : "NO");
                sprintf(response, "서버: %s의 설정 밝기를 %d(으)로 변경했습니다 (%s).\n", 
                        led_color_names[led_num], val, is_on ? "실시간 반영됨" : "소등 상태 유지");
                write(fd, response, strlen(response));

                session->current_session = SESSION_LED_ACTION; // 성공 시 상위 세션으로 자동 퇴장
            } else {
                char* invalid_pwm = "서버: 잘못된 밝기 값입니다. (0~255 범위 입력)\n";
                write(fd, invalid_pwm, strlen(invalid_pwm));
            }
            break;
        }

        case SESSION_BUZ: {
            CHECK_LOCK();
            if (strcmp(input, "1") == 0 || strcasecmp(input, "on") == 0) {
                start_buz_melody();
                write_daemon_log("INFO", "[Socket: %d] 부저 제어: 작동 ON (멜로디 스레드 가동)\n", fd);
                char* buz_on_msg = "서버: 부저 작동 ON (멜로디 연주 중)\n";
                write(fd, buz_on_msg, strlen(buz_on_msg));
            }
            else if (strcmp(input, "0") == 0 || strcasecmp(input, "off") == 0) {
                stop_buz_thread();
                write_daemon_log("INFO", "[Socket: %d] 부저 제어: 작동 OFF\n", fd);
                char* buz_off_msg = "서버: 부저 작동 OFF\n";
                write(fd, buz_off_msg, strlen(buz_off_msg));
            }
            else {
                char* invalid_buz = "서버: 잘못된 값입니다. (1 또는 0 입력)\n";
                write(fd, invalid_buz, strlen(invalid_buz));
            }
            break;
        }

        case SESSION_YL40:
            if (strcmp(input, "1") == 0) {
                pthread_mutex_lock(readIO->mutex);
                sprintf(response, "서버: 조도=%d, 온도=%.2fC, 가변저항=%d, LED(D1)=%d\n",
                        readIO->read_ldr, readIO->read_therm, readIO->read_pot, readIO->d1_led_state);
                pthread_mutex_unlock(readIO->mutex);
                write(fd, response, strlen(response));
            }
            else if (strcmp(input, "2") == 0) {
                session->current_session = SESSION_YL40_AUTO;
            }
            else if (strcmp(input, "3") == 0) {
                session->current_session = SESSION_YL40_THRESHOLD;
            }
            else if (strcmp(input, "4") == 0) {
                session->current_session = SESSION_YL40_D1;
            }
            else {
                char* invalid_yl_menu = "서버: 잘못된 메뉴 선택입니다.\n";
                write(fd, invalid_yl_menu, strlen(invalid_yl_menu));
            }
            break;

        case SESSION_YL40_AUTO: {
            CHECK_LOCK();
            if (strcmp(input, "1") == 0 || strcasecmp(input, "on") == 0) {
                pthread_mutex_lock(readIO->mutex);
                readIO->auto_mode = 1;
                pthread_mutex_unlock(readIO->mutex);
                write_daemon_log("INFO", "[Socket: %d] YL-40 제어: 조도 자동 제어(오토 모드) 활성화\n", fd);
                char* auto_on = "서버: 조도 자동 제어(오토 모드)를 활성화했습니다.\n";
                write(fd, auto_on, strlen(auto_on));
            }
            else if (strcmp(input, "0") == 0 || strcasecmp(input, "off") == 0) {
                pthread_mutex_lock(readIO->mutex);
                readIO->auto_mode = 0;
                readIO->led_pwm_state[1] = 0;
                readIO->led_pwm_state[2] = 0;
                readIO->led_pwm_state[3] = 0;
                pthread_mutex_unlock(readIO->mutex);

                stop_led_thread(1);
                stop_led_thread(2);
                stop_led_thread(3);

                softPwmWrite(LED1_PIN, 0);
                pwmWrite(LED2_PIN, 0);
                pwmWrite(LED3_PIN, 0);

                write_daemon_log("INFO", "[Socket: %d] YL-40 제어: 조도 자동 제어(오토 모드) 비활성화 및 3색 LED 소등\n", fd);
                char* auto_off = "서버: 조도 자동 제어(오토 모드)를 비활성화했습니다 (3색 LED 소등).\n";
                write(fd, auto_off, strlen(auto_off));
            }
            else {
                char* invalid_auto = "서버: 잘못된 값입니다. (1 또는 0 입력)\n";
                write(fd, invalid_auto, strlen(invalid_auto));
            }
            break;
        }

        case SESSION_YL40_THRESHOLD: {
            CHECK_LOCK();
            int val = atoi(input);
            if (val >= 0 && val <= 255) {
                pthread_mutex_lock(readIO->mutex);
                readIO->threshold = val;
                pthread_mutex_unlock(readIO->mutex);
                write_daemon_log("INFO", "[Socket: %d] YL-40 제어: 조도 판단 임계값 변경 (설정값: %d)\n", fd, val);
                sprintf(response, "서버: 조도 판단 임계값(Threshold)을 %d(으)로 설정했습니다.\n", val);
                write(fd, response, strlen(response));
            } else {
                char* invalid_thresh = "서버: 잘못된 임계값입니다. (0~255 범위 입력)\n";
                write(fd, invalid_thresh, strlen(invalid_thresh));
            }
            break;
        }

        case SESSION_YL40_D1:
            if (strcmp(input, "1") == 0) {
                session->current_session = SESSION_YL40_D1_ONOFF;
            }
            else if (strcmp(input, "2") == 0) {
                session->current_session = SESSION_YL40_D1_PWM;
            }
            else if (strcmp(input, "3") == 0) {
                CHECK_LOCK();
                pthread_mutex_lock(readIO->mutex);
                readIO->auto_mode = 0;
                pthread_mutex_unlock(readIO->mutex);
                
                start_yl40_d1_blink(readIO->yl40_fd);
                
                write_daemon_log("INFO", "[Socket: %d] YL-40 제어: D1 LED BLINK 효과 구동\n", fd);
                char* blink_d1_msg = "서버: D1 LED BLINK 효과를 구동합니다.\n";
                write(fd, blink_d1_msg, strlen(blink_d1_msg));
            }
            else {
                char* invalid_d1_opt = "서버: 잘못된 선택입니다.\n";
                write(fd, invalid_d1_opt, strlen(invalid_d1_opt));
            }
            break;

        case SESSION_YL40_D1_ONOFF: {
            CHECK_LOCK();
            pthread_mutex_lock(readIO->mutex);
            readIO->auto_mode = 0;
            pthread_mutex_unlock(readIO->mutex);
            stop_led_thread(0);

            if (strcmp(input, "1") == 0 || strcasecmp(input, "on") == 0) {
                pthread_mutex_lock(readIO->mutex);
                int val = readIO->d1_led_preset;
                readIO->d1_led_state = val;
                write_pcf8591(readIO->yl40_fd, YL40_AOUT, val);
                pthread_mutex_unlock(readIO->mutex);
                write_daemon_log("INFO", "[Socket: %d] YL-40 제어: D1 LED 점등(ON, 밝기: %d)\n", fd, val);
                sprintf(response, "서버: D1 LED를 점등(ON)했습니다 (밝기: %d).\n", val);
                write(fd, response, strlen(response));
            }
            else if (strcmp(input, "0") == 0 || strcasecmp(input, "off") == 0) {
                pthread_mutex_lock(readIO->mutex);
                readIO->d1_led_state = 0;
                write_pcf8591(readIO->yl40_fd, YL40_AOUT, 0);
                pthread_mutex_unlock(readIO->mutex);
                write_daemon_log("INFO", "[Socket: %d] YL-40 제어: D1 LED 소등(OFF)\n", fd);
                char* d1_off = "서버: D1 LED를 소등(OFF)했습니다.\n";
                write(fd, d1_off, strlen(d1_off));
            }
            else {
                char* invalid_d1_onoff = "서버: 잘못된 값입니다. (1 또는 0 입력)\n";
                write(fd, invalid_d1_onoff, strlen(invalid_d1_onoff));
            }
            break;
        }

        case SESSION_YL40_D1_PWM: {
            CHECK_LOCK();
            int val = atoi(input);
            if (val >= 0 && val <= 255) {
                pthread_mutex_lock(readIO->mutex);
                readIO->auto_mode = 0;
                pthread_mutex_unlock(readIO->mutex);
                stop_led_thread(0);

                pthread_mutex_lock(readIO->mutex);
                readIO->d1_led_preset = val; // 프리셋 항상 저장
                int is_on = (readIO->d1_led_state > 0);
                if (is_on) {
                    write_pcf8591(readIO->yl40_fd, YL40_AOUT, val);
                    readIO->d1_led_state = val;
                }
                pthread_mutex_unlock(readIO->mutex);

                write_daemon_log("INFO", "[Socket: %d] YL-40 제어: D1 LED 설정 밝기 변경 (설정값: %d, 실시간 반영 여부: %s)\n", 
                                 fd, val, is_on ? "YES" : "NO");
                sprintf(response, "서버: D1 LED의 설정 밝기를 %d(으)로 변경했습니다 (%s).\n", 
                        val, is_on ? "실시간 반영됨" : "소등 상태 유지");
                write(fd, response, strlen(response));

                session->current_session = SESSION_YL40_D1; // 성공 시 상위 세션으로 자동 퇴장
            } else {
                char* invalid_d1_pwm = "서버: 잘못된 밝기 값입니다. (0~255 범위 입력)\n";
                write(fd, invalid_d1_pwm, strlen(invalid_d1_pwm));
            }
            break;
        }

        case SESSION_SEG: {
            CHECK_LOCK();
            int seconds = atoi(input);
            if (seconds >= 0 && seconds <= 9) {
                start_seg_timer(seconds, fd, session, readIO);
                
                write_daemon_log("INFO", "[Socket: %d] 7-Segment 제어: %d초 타이머 구동 시작\n", fd, seconds);
                sprintf(response, "서버: %d초 타이머를 비동기로 구동합니다.\n", seconds);
                write(fd, response, strlen(response));
                
                return 1;
            } else {
                char* invalid_seg = "서버: 잘못된 타이머 시간입니다. (0~9 범위 입력)\n";
                write(fd, invalid_seg, strlen(invalid_seg));
            }
            break;
        }
    }

    send_menu_and_prompt(fd, session, readIO);
    return 1;
}

client_session_t* add_client_session(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sessions[i].fd == -1) {
            client_sessions[i].fd = fd;
            client_sessions[i].current_session = SESSION_MAIN;
            client_sessions[i].selected_led = 0;
            return &client_sessions[i];
        }
    }
    return NULL;
}

client_session_t* get_client_session(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sessions[i].fd == fd) {
            return &client_sessions[i];
        }
    }
    return NULL;
}

void remove_client_session(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sessions[i].fd == fd) {
            client_sessions[i].fd = -1;
            client_sessions[i].current_session = SESSION_MAIN;
            client_sessions[i].selected_led = 0;
            break;
        }
    }
}

void daemon_server(readIO_t* readIO) {
    set_daemon_log_filename(readIO->log_filename);
    write_daemon_log("INFO", "서버 데몬이 포트 %d에서 정상 기동되었습니다.\n", PORT);
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t addr_sz;
    
    int epfd, event_cnt;
    struct epoll_event *ep_events;
    struct epoll_event event;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sessions[i].fd = -1;
    }

    pthread_mutex_lock(readIO->mutex);
    readIO->active_client_fd = -1;
    pthread_mutex_unlock(readIO->mutex);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind() error");
        exit(1);
    }

    if (listen(serv_sock, 10) == -1) {
        perror("listen() error");
        exit(1);
    }

    epfd = epoll_create(50);
    ep_events = malloc(sizeof(struct epoll_event) * MAX_CLIENTS);

    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    while (1) {
        event_cnt = epoll_wait(epfd, ep_events, MAX_CLIENTS, -1);
        if (event_cnt == -1) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < event_cnt; i++) {
            if (ep_events[i].data.fd == serv_sock) {
                addr_sz = sizeof(clnt_addr);
                clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &addr_sz);
                
                client_session_t *sess = add_client_session(clnt_sock);
                char *clnt_ip = inet_ntoa(clnt_addr.sin_addr);
                if (sess == NULL) {
                    write_daemon_log("WARN", "접속 거부: 인원 초과 (IP: %s, Socket: %d)\n", clnt_ip, clnt_sock);
                    char* over_limit = "서버: 접속 인원 한도 초과입니다.\n";
                    write(clnt_sock, over_limit, strlen(over_limit));
                    close(clnt_sock);
                } else {
                    write_daemon_log("INFO", "클라이언트 접속 성공 (IP: %s, Socket: %d)\n", clnt_ip, clnt_sock);
                    event.events = EPOLLIN;
                    event.data.fd = clnt_sock;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                    
                    char* welcome = "서버: 장치 제어 데몬 세션에 오신 것을 환영합니다.\n";
                    write(clnt_sock, welcome, strlen(welcome));
                    send_menu_and_prompt(clnt_sock, sess, readIO);
                }
            } 
            else {
                int target_fd = ep_events[i].data.fd;
                char buf[MAXDATASIZE];
                int str_len = read(target_fd, buf, sizeof(buf) - 1);
                
                if (str_len <= 0) {
                    write_daemon_log("INFO", "클라이언트 비정상 끊김 또는 단선 감지 (Socket: %d)\n", target_fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, target_fd, NULL);
                    close(target_fd);

                    remove_client_session(target_fd);

                    pthread_mutex_lock(readIO->mutex);
                    if (readIO->active_client_fd == target_fd) {
                        readIO->active_client_fd = -1;
                        stop_all_io_operations();
                    }
                    pthread_mutex_unlock(readIO->mutex);
                } 
                else {
                    buf[str_len] = '\0';
                    client_session_t *sess = get_client_session(target_fd);
                    if (sess != NULL) {
                        int keep = process_client_input(sess, buf, readIO);
                        if (!keep) {
                            write_daemon_log("INFO", "클라이언트 정상 세션 해제 및 퇴장 완료 (Socket: %d)\n", target_fd);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, target_fd, NULL);
                            close(target_fd);
                            remove_client_session(target_fd);

                            pthread_mutex_lock(readIO->mutex);
                            if (readIO->active_client_fd == target_fd) {
                                readIO->active_client_fd = -1;
                                stop_all_io_operations();
                            }
                            pthread_mutex_unlock(readIO->mutex);
                        }
                    }
                }
            }
        }
    }

    close(serv_sock);
    free(ep_events);
}

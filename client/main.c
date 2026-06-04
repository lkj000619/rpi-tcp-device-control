#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAXDATASIZE 256

// 세션 상태를 정의하는 열거형 (오토 모드 전용 세션 추가)
enum SessionState {
    SESSION_MAIN = 0,
    SESSION_LED_SELECT,  
    SESSION_LED_ACTION,  
    SESSION_LED_ONOFF,      
    SESSION_LED_PWM,     
    SESSION_BUZ,
    SESSION_YL40,
    SESSION_YL40_AUTO,      // <-- 오토 모드 제어 세션 추가
    SESSION_YL40_THRESHOLD, 
    SESSION_YL40_D1,
    SESSION_YL40_D1_ONOFF,       
    SESSION_YL40_D1_PWM,    
    SESSION_SEG
};

const char* led_names[] = {"", "빨간색(Soft PWM)", "노란색(Hard PWM)", "녹색(Hard PWM)"};

void cli_loop(int sd, struct sockaddr_in usr);
void print_menu(int current_session, int selected_led);
void print_prompt(int current_session, int selected_led);

int main(int argc, char *argv[])
{
    int sockfd;
    struct hostent *he;
    struct sockaddr_in server_addr;

    if(argc != 2) {
        fprintf(stderr, "사용법 : %s <서버 IP>\n", argv[0]);
        exit(1);
    }
    
    if((he = gethostbyname(argv[1])) == NULL) {
        perror("gethostbyname");
        exit(1);
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(60000);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(server_addr.sin_zero), '\0', 8);

    printf("서버(%s)에 연결을 시도합니다...\n", inet_ntoa(server_addr.sin_addr));

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))== -1) {
        perror("connect");
        exit(1);
    }

    printf("연결 성공!\n");
    cli_loop(sockfd, server_addr);
    close(sockfd);
    return 0;
}

void print_menu(int current_session, int selected_led) {
    printf("\n");
    switch (current_session) {
        case SESSION_MAIN:
            printf("========== [ 메인 메뉴 ] ==========\n");
            printf(" 1. LED 제어\n");
            printf(" 2. 부저 제어\n");
            printf(" 3. YL-40 모듈 제어\n");
            printf(" 4. 7-Segment 타이머 제어\n");
            printf("-1. 프로그램 종료\n");
            printf("-----------------------------------\n");
            printf(" 접속할 세션 번호를 입력하세요: \n");
            break;
            
        case SESSION_LED_SELECT:
            printf("---------- [ LED 대상 선택 ] ----------\n");
            printf(" [1] 빨간색 LED (Soft PWM)\n");
            printf(" [2] 노란색 LED (Hard PWM)\n");
            printf(" [3] 녹색 LED   (Hard PWM)\n");
            printf(" --------------------------------------\n");
            printf(" 제어할 LED 번호를 선택하세요 (1~3):\n");
            printf(" (-1. 이전 단계)\n");
            break;

        case SESSION_LED_ACTION:
            printf("------- [ %s 전용 제어 ] -------\n", led_names[selected_led]);
            printf("  1. 단순 제어 : ON/OFF 설정 세션 진입\n");
            printf("  2. 밝기 조절 : PWM 상세 조절 세션 진입\n");
            printf("  3. BLINK ( 깜빡임 )\n");
            printf("  4. FADE ( 숨쉬기 효과 )\n");
            printf(" ------------------------------------------\n");
            printf(" 메뉴 번호를 입력하세요:\n");
            printf(" (-1. 이전 단계)\n");
            break;

        case SESSION_LED_ONOFF:
            printf("------- [ %s ON/OFF ] -------\n", led_names[selected_led]);
            printf(" - 입력형식 : <ON 1 / OFF 0>\n");
            printf(" - 예시 : 1 (켜기) 또는 0 (끄기)\n");
            printf(" ------------------------------------------\n");
            printf(" 명령어 입력:\n");
            printf(" (-1. 이전 단계)\n");
            break;

        case SESSION_LED_PWM:
            printf("------- [ %s 밝기(PWM) 상세 조절 ] -------\n", led_names[selected_led]);
            printf(" - 0 부터 255 사이의 값을 입력하세요.\n");
            printf(" - (255 = 최대 밝기 / 128 = 중간 밝기 / 0 = 꺼짐)\n");
            printf(" ------------------------------------------\n");
            printf(" 원하는 밝기 숫자를 입력하세요:\n");
            printf(" (-1. 이전 단계)\n");
            break;

        case SESSION_BUZ:
            printf("--------- [ 부저 세션 ] ---------\n");
            printf(" - 입력형식 : <ON 1 / OFF 0>\n");
            printf(" - 예시 : 1 (켜기) 또는 0 (끄기)\n");
            printf(" ---------------------------------\n");
            printf(" 명령어 입력:\n");
            printf(" (-1. 이전 단계)\n");
            break;
            
        case SESSION_YL40:
            printf("-------- [ YL-40 세션 ] --------\n");
            printf("  1. 센서 읽기 (READ)\n");
            printf("  2. 오토 모드 제어 세션 진입\n"); // 변경됨
            printf("  3. 오토 모드 기준값(Threshold) 설정 세션 진입\n");
            printf("  4. D1 LED 상세 제어 세션 진입\n");
            printf(" --------------------------------\n");
            printf(" 메뉴 번호를 입력하세요:\n");
            printf(" (-1. 이전 단계)\n");
            break;

        // [추가] 오토 모드 전용 세션
        case SESSION_YL40_AUTO:
            printf("------- [ 오토 모드 ON/OFF 제어 ] -------\n");
            printf(" - [!] 서버로부터 현재 상태를 수신 중입니다...\n");
            printf(" - 입력형식 : <ON 1 / OFF 0>\n");
            printf(" - 예시 : 1 (켜기) 또는 0 (끄기)\n");
            printf(" ----------------------------------------\n");
            printf(" 변경할 상태를 입력하세요:\n");
            printf(" (-1. 이전 단계)\n");
            break;

        case SESSION_YL40_THRESHOLD:
            printf("---- [ 오토 모드 기준값(Threshold) 설정 ] ----\n");
            printf(" - [!] 서버로부터 현재 기준값을 수신 중입니다...\n");
            printf(" - 어두움을 판단할 조도 센서 기준값을 입력하세요 (0~255)\n");
            printf(" - 값이 클수록 더 어두워져야 LED가 켜집니다.\n");
            printf(" ----------------------------------------------\n");
            printf(" 설정할 숫자(기준값)를 입력하세요:\n");
            printf(" (-1. 이전 단계)\n");
            break;

        case SESSION_YL40_D1:
            printf("------- [ YL-40 D1 LED 상세 제어 ] -------\n");
            printf("  1. 단순 제어 : D1 LED ON/OFF 세션 진입\n");
            printf("  2. 밝기 조절 : PWM 상세 조절 세션 진입\n");
            printf("  3. 특수 효과 : BLINK (깜빡임)\n");
            printf(" ------------------------------------------\n");
            printf(" 메뉴 번호를 입력하세요:\n");
            printf(" (-1. 이전 단계)\n");
            break;
        
        case SESSION_YL40_D1_ONOFF:
            printf("------- [ YL-40 D1 LED ON/OFF ] -------\n");
            printf(" - 입력형식 : <ON 1 / OFF 0>\n");
            printf(" - 예시 : 1 (켜기) 또는 0 (끄기)\n");
            printf(" ------------------------------------------\n");
            printf(" 명령어 입력:\n");
            printf(" (-1. 이전 단계)\n");
            break;

        case SESSION_YL40_D1_PWM:
            printf("------- [ YL-40 D1 밝기(PWM) 상세 조절 ] -------\n");
            printf(" - 0 부터 255 사이의 값을 입력하세요.\n");
            printf(" - (255 = 최대 밝기 / 128 = 중간 밝기 / 0 = 꺼짐)\n");
            printf(" ------------------------------------------\n");
            printf(" 원하는 밝기 숫자를 입력하세요:\n");
            printf(" (-1. 이전 단계)\n");
            break;
            
        case SESSION_SEG:
            printf("------ [ 7-Segment 세션 ] ------\n");
            printf(" 입력형식: <0~9> (초 단위 카운트다운)\n");
            printf(" 예시: 5\n");
            printf(" (-1. 이전 단계)\n");
            break;
    }
}

void print_prompt(int current_session, int selected_led) {
    switch (current_session) {
        case SESSION_MAIN:           printf("[ MAIN ] > "); break;
        case SESSION_LED_SELECT:     printf("[ LED ] > "); break;
        case SESSION_LED_ACTION: 
            if (selected_led == 1) printf("[ LED:빨간 ] > ");
            else if (selected_led == 2) printf("[ LED:노란 ] > ");
            else if (selected_led == 3) printf("[ LED:녹색 ] > ");
            break;
        case SESSION_LED_ONOFF: 
            if (selected_led == 1) printf("[ 빨간(ON/OFF) ] > ");
            else if (selected_led == 2) printf("[ 노란(ON/OFF) ] > ");
            else if (selected_led == 3) printf("[ 녹색(ON/OFF) ] > ");
            break;
        case SESSION_LED_PWM:
            if (selected_led == 1) printf("[ 빨간(PWM) ] > ");
            else if (selected_led == 2) printf("[ 노란(PWM) ] > ");
            else if (selected_led == 3) printf("[ 녹색(PWM) ] > ");
            break;
        case SESSION_BUZ:            printf("[ BUZ ] > "); break;
        case SESSION_YL40:           printf("[ YL-40 ] > "); break;
        case SESSION_YL40_AUTO:      printf("[ YL-40:AUTO설정 ] > "); break;
        case SESSION_YL40_THRESHOLD: printf("[ YL-40:기준값설정 ] > "); break;
        case SESSION_YL40_D1:        printf("[ YL-40:D1 ] > "); break;
        case SESSION_YL40_D1_ONOFF:  printf("[ YL-40:D1(ON/OFF) ] > "); break;
        case SESSION_YL40_D1_PWM:    printf("[ YL-40:D1(PWM) ] > "); break;
        case SESSION_SEG:            printf("[ SEG ] > "); break;
    }
    fflush(stdout); 
}

void cli_loop(int sd, struct sockaddr_in usr){
    fd_set fdset, fdset1;
    char buf[MAXDATASIZE];
    char send_buf[MAXDATASIZE]; 
    
    int current_session = SESSION_MAIN;
    int selected_led = 0;

    FD_ZERO(&fdset);
    FD_SET(0, &fdset); 
    FD_SET(sd, &fdset);
    fdset1 = fdset;

    print_menu(current_session, selected_led);
    print_prompt(current_session, selected_led);

    while (1){
        fdset = fdset1;
        
        if(select(sd + 1, &fdset, NULL, NULL, NULL) == -1) break;
        
        if(FD_ISSET(0, &fdset)){
            if (fgets(buf, sizeof(buf), stdin) == NULL) break;
            buf[strcspn(buf, "\r\n")] = 0;
            
            if(strlen(buf) == 0) { 
                print_prompt(current_session, selected_led);
                continue;
            }

            if(current_session == SESSION_MAIN && (strcmp(buf, "-1") == 0 || strcmp(buf, "exit") == 0)) {
                printf("클라이언트를 종료합니다.\n");
                break;
            }

            // 라우팅 로직 (SESSION_YL40_AUTO 추가)
            if(strcmp(buf, "exit") == 0 || strcmp(buf, "-1") == 0) {
                if (current_session == SESSION_LED_PWM || current_session == SESSION_LED_ONOFF) {
                    current_session = SESSION_LED_ACTION;
                } 
                else if (current_session == SESSION_LED_ACTION) {
                    current_session = SESSION_LED_SELECT;
                } 
                else if (current_session == SESSION_YL40_THRESHOLD || current_session == SESSION_YL40_D1 || current_session == SESSION_YL40_AUTO) {
                    current_session = SESSION_YL40;
                } 
                else if (current_session == SESSION_YL40_D1_PWM || current_session == SESSION_YL40_D1_ONOFF) {
                    current_session = SESSION_YL40_D1;
                } 
                else {
                    current_session = SESSION_MAIN;
                }
                
                print_menu(current_session, selected_led);
                print_prompt(current_session, selected_led);
                continue;
            }

            memset(send_buf, 0, sizeof(send_buf));
            
            if (current_session == SESSION_MAIN) {
                if (strcmp(buf, "1") == 0) current_session = SESSION_LED_SELECT;
                else if (strcmp(buf, "2") == 0) current_session = SESSION_BUZ;
                else if (strcmp(buf, "3") == 0) current_session = SESSION_YL40;
                else if (strcmp(buf, "4") == 0) current_session = SESSION_SEG;
                else printf("잘못된 선택입니다.\n");
                
                print_menu(current_session, selected_led);
            } 
            else if (current_session == SESSION_LED_SELECT) {
                if (strcmp(buf, "1") == 0 || strcmp(buf, "2") == 0 || strcmp(buf, "3") == 0) {
                    selected_led = atoi(buf);
                    current_session = SESSION_LED_ACTION;
                    print_menu(current_session, selected_led);
                } else {
                    printf("올바른 LED 번호(1~3)를 입력해주세요.\n");
                }
            }
            else {
                if (current_session == SESSION_LED_ACTION) {
                    if (strcmp(buf, "1") == 0) {
                        current_session = SESSION_LED_ONOFF;
                        print_menu(current_session, selected_led);
                    } 
                    else if (strcmp(buf, "2") == 0) {
                        current_session = SESSION_LED_PWM;
                        print_menu(current_session, selected_led);
                    } 
                    else if (strcmp(buf, "3") == 0) {
                        sprintf(send_buf, "LED %d BLINK", selected_led);
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } 
                    else if (strcmp(buf, "4") == 0) {
                        sprintf(send_buf, "LED %d FADE", selected_led);
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } 
                    else {
                        printf("잘못된 입력입니다. (1~4 또는 -1)\n");
                    }
                } 
                else if (current_session == SESSION_LED_ONOFF) {
                    char mapped_cmd[10] = {0};
                    if (strcmp(buf, "1") == 0 || strcasecmp(buf, "on") == 0) strcpy(mapped_cmd, "ON");
                    else if (strcmp(buf, "0") == 0 || strcasecmp(buf, "off") == 0) strcpy(mapped_cmd, "OFF");

                    if (strlen(mapped_cmd) > 0) {
                        sprintf(send_buf, "LED %d %s", selected_led, mapped_cmd);
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } else printf("잘못된 입력입니다. (1: 켜기 / 0: 끄기)\n");
                }
                else if (current_session == SESSION_LED_PWM) {
                    sprintf(send_buf, "LED %d PWM %s", selected_led, buf);
                    if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                } 
                else if (current_session == SESSION_BUZ) {
                    char mapped_cmd[10] = {0};
                    if (strcmp(buf, "1") == 0 || strcasecmp(buf, "on") == 0) strcpy(mapped_cmd, "ON");
                    else if (strcmp(buf, "0") == 0 || strcasecmp(buf, "off") == 0) strcpy(mapped_cmd, "OFF");

                    if (strlen(mapped_cmd) > 0) {
                        sprintf(send_buf, "BUZ %s", mapped_cmd);
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } else printf("잘못된 입력입니다. (1: 켜기 / 0: 끄기)\n");
                } 
                else if (current_session == SESSION_YL40) {
                    if (strcmp(buf, "1") == 0) {
                        sprintf(send_buf, "YL40 READ");
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } 
                    else if (strcmp(buf, "2") == 0) {
                        current_session = SESSION_YL40_AUTO;
                        print_menu(current_session, selected_led);
                        // [핵심] 진입 시 서버에 현재 오토 모드 상태 요청
                        sprintf(send_buf, "YL40 GET_AUTO");
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } 
                    else if (strcmp(buf, "3") == 0) {
                        current_session = SESSION_YL40_THRESHOLD;
                        print_menu(current_session, selected_led);
                        // [핵심] 진입 시 서버에 현재 스래쉬홀드 값 요청
                        sprintf(send_buf, "YL40 GET_THRESHOLD");
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } 
                    else if (strcmp(buf, "4") == 0) {
                        current_session = SESSION_YL40_D1;
                        print_menu(current_session, selected_led);
                    } 
                    else {
                        printf("잘못된 입력입니다. (1~4 또는 -1)\n");
                    }
                }
                else if (current_session == SESSION_YL40_AUTO) {
                    char mapped_cmd[10] = {0};
                    if (strcmp(buf, "1") == 0 || strcasecmp(buf, "on") == 0) strcpy(mapped_cmd, "ON");
                    else if (strcmp(buf, "0") == 0 || strcasecmp(buf, "off") == 0) strcpy(mapped_cmd, "OFF");

                    if (strlen(mapped_cmd) > 0) {
                        sprintf(send_buf, "YL40 AUTO %s", mapped_cmd);
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } else printf("잘못된 입력입니다. (1: 켜기 / 0: 끄기)\n");
                }
                else if (current_session == SESSION_YL40_THRESHOLD) {
                    sprintf(send_buf, "YL40 THRESHOLD %s", buf);
                    if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                }
                else if (current_session == SESSION_YL40_D1) {
                    if (strcmp(buf, "1") == 0) {
                        current_session = SESSION_YL40_D1_ONOFF;
                        print_menu(current_session, selected_led);
                    } 
                    else if (strcmp(buf, "2") == 0) {
                        current_session = SESSION_YL40_D1_PWM;
                        print_menu(current_session, selected_led);
                    } 
                    else if (strcmp(buf, "3") == 0) {
                        sprintf(send_buf, "YL40 D1 BLINK");
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    }
                    else {
                        printf("잘못된 입력입니다. (1~3 또는 -1)\n");
                    }
                }
                else if (current_session == SESSION_YL40_D1_ONOFF) {
                    char mapped_cmd[10] = {0};
                    if (strcmp(buf, "1") == 0 || strcasecmp(buf, "on") == 0) strcpy(mapped_cmd, "ON");
                    else if (strcmp(buf, "0") == 0 || strcasecmp(buf, "off") == 0) strcpy(mapped_cmd, "OFF");

                    if (strlen(mapped_cmd) > 0) {
                        sprintf(send_buf, "YL40 D1 %s", mapped_cmd);
                        if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                    } else printf("잘못된 입력입니다. (1: 켜기 / 0: 끄기)\n");
                }
                else if (current_session == SESSION_YL40_D1_PWM) {
                    sprintf(send_buf, "YL40 D1 PWM %s", buf);
                    if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                }
                else if (current_session == SESSION_SEG) {
                    sprintf(send_buf, "SEG %s", buf);
                    if(send(sd, send_buf, strlen(send_buf), 0) == -1) break;
                }
            }
            
            print_prompt(current_session, selected_led);
        }
        
        else if(FD_ISSET(sd, &fdset)){
            int bytes;
            if((bytes = recv(sd, buf, MAXDATASIZE-1, 0)) == -1){
                perror("recv");
                break;
            }
            else if(bytes == 0){
                printf("\n서버와의 연결이 끊어졌습니다 (Disconnected).\n");
                break;
            }
            
            buf[bytes] = '\0';
            printf("\r[ SERVER ] > %s\n", buf);

            if (current_session == SESSION_SEG && strstr(buf, "타이머 종료") != NULL) {
                printf("\r[ 시스템 ] 타이머 동작이 완료되어 메인 메뉴로 복귀합니다.\n");
                current_session = SESSION_MAIN;
                print_menu(current_session, selected_led);
            }

            print_prompt(current_session, selected_led);
        }
    }
}
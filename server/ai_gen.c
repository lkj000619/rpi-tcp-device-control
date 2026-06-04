#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 60000
#define MAX_CLIENTS 20

// 타이머 스레드에 전달할 인자 구조체
typedef struct {
    int fd;
    int time;
} TimerArg;

// 7-Segment 가상 카운트다운 스레드
void *mock_timer_thread(void *arg) {
    TimerArg *t_arg = (TimerArg *)arg;
    int client_fd = t_arg->fd;
    int start_time = t_arg->time;
    free(t_arg);

    char msg[100];
    for (int i = start_time; i >= 0; i--) {
        sprintf(msg, "[백그라운드] 가상 7-Segment 표시: %d초\n", i);
        write(client_fd, msg, strlen(msg));
        sleep(1);
    }
    
    sprintf(msg, "[백그라운드] 타이머 종료! 가상 부저 ON\n");
    write(client_fd, msg, strlen(msg));
    return NULL;
}

int main(void) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t addr_sz;
    
    int epfd, event_cnt;
    struct epoll_event *ep_events;
    struct epoll_event event;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    
    // 포트 재사용 설정 (서버 재시작 시 bind 에러 방지)
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

    listen(serv_sock, 5);

    epfd = epoll_create(50);
    ep_events = malloc(sizeof(struct epoll_event) * MAX_CLIENTS);

    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    printf("가상 테스트 서버가 시작되었습니다. (포트: %d)\n", PORT);

    while (1) {
        event_cnt = epoll_wait(epfd, ep_events, MAX_CLIENTS, -1);
        if (event_cnt == -1) {
            puts("epoll_wait() error");
            break;
        }

        for (int i = 0; i < event_cnt; i++) {
            // 1. 신규 클라이언트 접속
            if (ep_events[i].data.fd == serv_sock) {
                addr_sz = sizeof(clnt_addr);
                clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &addr_sz);
                
                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                printf("[접속] 새로운 클라이언트 연결 (fd: %d)\n", clnt_sock);
            } 
            // 2. 클라이언트로부터 메시지 수신
            else {
                int target_fd = ep_events[i].data.fd;
                char buf[256];
                int str_len = read(target_fd, buf, sizeof(buf) - 1);
                
                if (str_len <= 0) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, target_fd, NULL);
                    close(target_fd);
                    printf("[종료] 클라이언트 연결 해제 (fd: %d)\n", target_fd);
                } else {
                    buf[str_len] = '\0';
                    printf("[수신 fd:%d] %s\n", target_fd, buf);

                    char cmd[20] = {0}, arg1[20] = {0}, arg2[20] = {0}, arg3[20] = {0};
                    char response[256] = {0};
                    sscanf(buf, "%s %s %s %s", cmd, arg1, arg2, arg3);

                    // --- 명령어 모의(Mock) 처리 ---
                    if (strcmp(cmd, "LED") == 0) {
                        if (strcmp(arg2, "PWM") == 0) {
                            sprintf(response, "서버: [가상제어] LED %s번 밝기를 %s(으)로 설정했습니다.", arg1, arg3);
                        } else {
                            sprintf(response, "서버: [가상제어] LED %s번 상태를 %s(으)로 변경했습니다.", arg1, arg2);
                        }
                    } 
                    else if (strcmp(cmd, "BUZ") == 0) {
                        sprintf(response, "서버: [가상제어] 부저를 %s 했습니다.", arg1);
                    } 
                    else if (strcmp(cmd, "YL40") == 0) {
                        if (strcmp(arg1, "READ") == 0) {
                            sprintf(response, "서버: [가상제어] 조도=198, 온도=24.5C, 가변저항=120");
                        } else {
                            sprintf(response, "서버: [가상제어] YL-40 %s %s 적용 완료", arg1, arg2);
                        }
                    } 
                    else if (strcmp(cmd, "SEG") == 0) {
                        int time_val = atoi(arg1);
                        sprintf(response, "서버: [가상제어] %d초 타이머를 스레드로 실행합니다.", time_val);
                        
                        // 비동기 스레드 생성 (UI 겹침 방지 테스트용)
                        TimerArg *t_arg = malloc(sizeof(TimerArg));
                        t_arg->fd = target_fd;
                        t_arg->time = time_val;
                        pthread_t t_id;
                        pthread_create(&t_id, NULL, mock_timer_thread, (void *)t_arg);
                        pthread_detach(t_id);
                    } 
                    else {
                        sprintf(response, "서버: 알 수 없는 명령입니다.");
                    }

                    // 클라이언트에게 결과 응답
                    write(target_fd, response, strlen(response));
                }
            }
        }
    }
    
    close(serv_sock);
    free(ep_events);
    return 0;
}
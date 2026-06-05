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
#include <signal.h>

#define MAXDATASIZE 1024

int keep_running = 1;
int sockfd = -1;

// 시그널 핸들러: SIGINT(Ctrl+C) 수신 시에만 연결을 닫고 안전하게 종료합니다.
void sig_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n[시스템] SIGINT(Ctrl+C) 감지. 클라이언트를 안전하게 종료합니다.\n");
        keep_running = 0;
        if (sockfd != -1) {
            close(sockfd);
            sockfd = -1;
        }
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    struct hostent *he;
    struct sockaddr_in server_addr;
    struct sigaction sa;

    if(argc != 2) {
        fprintf(stderr, "사용법 : %s <서버 IP>\n", argv[0]);
        exit(1);
    }
    
    // 시그널 처리 설정: INT 신호에만 반응하도록 핸들러 등록
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }

    // 터미널 비정상 강제 종료를 막기 위해 SIGHUP, SIGTERM, SIGQUIT, SIGTSTP 신호들은 명시적으로 무시 처리
    sa.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    if((he = gethostbyname(argv[1])) == NULL) {
        perror("gethostbyname");
        exit(1);
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(server_addr.sin_zero), '\0', 8);

    printf("서버(%s)에 연결을 시도합니다...\n", inet_ntoa(server_addr.sin_addr));

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))== -1) {
        perror("connect");
        exit(1);
    }

    printf("연결 성공! 텔넷 터미널 세션이 시작되었습니다.\n");

    fd_set rset, rset_save;
    FD_ZERO(&rset_save);
    FD_SET(0, &rset_save);       // 표준 입력 감시
    FD_SET(sockfd, &rset_save);  // 소켓 연결 감시

    char buf[MAXDATASIZE];

    while (keep_running) {
        rset = rset_save;
        
        if (select(sockfd + 1, &rset, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) continue; // 시그널 인터럽트에 따른 중단은 계속 루프 실행
            perror("select");
            break;
        }

        // 1. 소켓 수신 처리: 서버가 밀어준 프롬프트/메뉴 화면 텍스트를 그대로 출력
        if (FD_ISSET(sockfd, &rset)) {
            int n;
            if ((n = recv(sockfd, buf, MAXDATASIZE - 1, 0)) <= 0) {
                if (n == 0) {
                    printf("\n서버와 연결이 끊어졌습니다 (Disconnected).\n");
                } else {
                    perror("recv");
                }
                break;
            }
            buf[n] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }

        // 2. 키보드 입력 처리: 입력된 문자열을 서버로 다이렉트 전송
        if (FD_ISSET(0, &rset)) {
            if (fgets(buf, sizeof(buf), stdin) == NULL) {
                break;
            }
            int len = strlen(buf);
            if (send(sockfd, buf, len, 0) == -1) {
                perror("send");
                break;
            }
        }
    }

    if (sockfd != -1) {
        close(sockfd);
    }
    return 0;
}
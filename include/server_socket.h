#ifndef SERVER_SOCKET_H
#define SERVER_SOCKET_H

#include "io_control.h" // readIO_t 구조체 및 기타 공유 자원 정의 필요

#define MAX_CLIENTS 20
#define MAXDATASIZE 256

// 세션 상태 열거형
enum SessionState {
    SESSION_MAIN = 0,
    SESSION_LED_SELECT,  
    SESSION_LED_ACTION,  
    SESSION_LED_ONOFF,      
    SESSION_LED_PWM,     
    SESSION_BUZ,
    SESSION_YL40,
    SESSION_YL40_AUTO,
    SESSION_YL40_THRESHOLD, 
    SESSION_YL40_D1,
    SESSION_YL40_D1_ONOFF,       
    SESSION_YL40_D1_PWM,    
    SESSION_SEG
};

// 개별 클라이언트 세션 관리 구조체
typedef struct client_session_t {
    int fd;                  // 클라이언트 소켓 디스크립터
    int current_session;     // 현재 세션 상태 (enum SessionState)
    int selected_led;        // 선택된 LED 번호 (1~3)
} client_session_t;

void daemon_server(readIO_t* readIO);
void write_daemon_log(const char* level, const char* format, ...);
void set_daemon_log_filename(const char* filename);

client_session_t* add_client_session(int fd);
client_session_t* get_client_session(int fd);
void remove_client_session(int fd);
int process_client_input(client_session_t *session, char *input, readIO_t *readIO);
void send_menu_and_prompt(int fd, client_session_t *session, readIO_t *readIO);

#endif

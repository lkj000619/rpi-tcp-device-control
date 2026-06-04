#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <pthread.h>
#include <dlfcn.h>
#include "common.h"

void * server_thread(void *arg){
    void *handle;
    void (*fptr)(readIO_t *);
    char *error;

    handle = dlopen("./libRemoteIO.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        syslog(LOG_ERR, "dlopen failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }

    dlerror();    /* Clear any existing error */

    fptr = dlsym(handle, "daemon_server");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym failed: %s\n", error);
        syslog(LOG_ERR, "dlsym failed: %s", error);
        exit(EXIT_FAILURE);
    }
    fptr(arg);

    dlclose(handle);
    pthread_exit(NULL);
}

void * IOread_thread(void *arg){
    void *handle;
    void (*fptr)(readIO_t *);
    char *error;

    handle = dlopen("./libRemoteIO.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        syslog(LOG_ERR, "dlopen failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }

    dlerror();    /* Clear any existing error */

    fptr = dlsym(handle, "IOread");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym failed: %s\n", error);
        syslog(LOG_ERR, "dlsym failed: %s", error);
        exit(EXIT_FAILURE);
    }
    fptr(arg);

    dlclose(handle);
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    struct sigaction sa; /* 시그널 처리를 위한 시그널 액션 */
    struct rlimit rl;
    int fd0, fd1, fd2, i;
    pid_t pid;
    char cwd[1024];

    pthread_t server_tid, IOread_tid;
    pthread_mutex_t IOread_mutex = PTHREAD_MUTEX_INITIALIZER;
    readIO_t read_data;

    if(argc < 2) {
        printf("Usage : %s command\n", argv[0]);
        return -1;
    }

    // 데몬화 전 라이브러리가 존재하는 현재의 원래 디렉터리 경로를 획득합니다.
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd()");
        return -1;
    }

    /* 파일 생성을 위한 마스크를 0으로 설정 */
    umask(0);

    /* 사용할 수 있는 최대의 파일 디스크립터 수 얻기 */
    if(getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("getlimit()");
    }

    if((pid = fork()) < 0) {
        perror("fork()");
        return -1;
    } else if(pid != 0) { /* 부모 프로세스는 종료한다. */
        return 0;
    }

    /* 터미널을 제어할 수 없도록 세션의 리더가 된다. */
    setsid();

    /* 터미널 제어와 관련된 시그널을 무시한다. */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction() : Can't ignore SIGHUP");
    }

    /* 작업 디렉터리를 원래 기동 폴더 경로로 설정하여 동적 라이브러리 로드가 가능하게 합니다. */
    if(chdir(cwd) < 0) {
        perror("chdir()");
    }

    /* 프로세스의 모든 파일 디스크립터를 닫는다. */
    if(rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }

    for(i = 0; i < (int)rl.rlim_max; i++) {
        close(i);
    }

    /* 파일 디스크립터 0, 1과 2를 /dev/null로 연결한다. */
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    /* 로그 출력을 위한 파일 로그를 연다. */
    openlog(argv[1], LOG_CONS, LOG_DAEMON);
    if(fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
        return -1;
    }

    /* 로그 파일에 정보 수준의 로그를 출력한다. */
    syslog(LOG_INFO, "Daemon Process Started");

    read_data.mutex = &IOread_mutex;

    // 각 스레드 생성 인자에 맞는 스레드 함수를 지정해 한 번만 기동합니다.
    if (pthread_create(&IOread_tid, NULL, IOread_thread, &read_data) != 0) {
        syslog(LOG_ERR, "pthread_create IOread_thread failed");
        closelog();
        return 1;
    }
    if (pthread_create(&server_tid, NULL, server_thread, &read_data) != 0) {
        syslog(LOG_ERR, "pthread_create server_thread failed");
        closelog();
        return 1;
    }

    pthread_join(IOread_tid, NULL);
    pthread_join(server_tid, NULL);

    /* 시스템 로그를 닫는다. */
    closelog();

    return 0;
}
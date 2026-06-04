#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common.h"

#define MAXDATASIZE 255
#define BACKLOG     10

void daemon_server(readIO_t* readIO){
    int sockfd, new_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int sin_size;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(60000);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    memset(&(server_addr.sin_zero),'\0',8);

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1){
        perror("bind");
        exit(1);
    }

    if(listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sin_size= sizeof(struct sockaddr_in);

    if((new_fd= accept(sockfd, (struct sockaddr*)&client_addr,&sin_size))== -1)  {
        perror("accept");
        // continue;
    }

    printf("server : got connection from %s \n",inet_ntoa(client_addr.sin_addr));

    command(new_fd, client_addr);

    close(new_fd);

    return;
}


void command(int sd, struct sockaddr_in usr){
    fd_set fdset, fdset1;
    char buf[MAXDATASIZE];

    FD_ZERO(&fdset);
    FD_SET(0, &fdset);
    FD_SET(sd,&fdset);

    fdset1 = fdset;

    printf("=================\n");
    printf("     CHATTING    ");
    printf("\n=================\n");

    while (1){
        printf("[ MY ] > ");
        fflush(stdout);
        fdset = fdset1;
        select(sd+1, &fdset, NULL, NULL, NULL);
        
        if(FD_ISSET(0, &fdset)){
            fgets(buf, sizeof(buf), stdin);
            if(send(sd, buf, strlen(buf), 0) == -1){
                perror("send");
                break;
            } 
            if(strncmp("exit",buf,4) == 0) break;
        }
        else if(FD_ISSET(sd, &fdset)){
            int bytes;
            if((bytes = recv(sd,buf,MAXDATASIZE-1,0)) == -1){
                perror("recv");
                break;
            }
            else if(bytes == 0){
                printf("Disconnected\n");
                break;
            }
            buf[bytes] = '\0';
            printf("\n[ %s ] > %s",(char*) inet_ntoa(usr.sin_addr), buf);

            if(strncmp("exit",buf,4) == 0) break;
        }
        else{
            printf("\nEXIT");
            break;
        }
    }
}
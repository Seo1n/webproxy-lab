#include "csapp.h"

int main(int argc, char **argv) { // 인자 개수, 인자 배열
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) { 
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]); // argv[0] = 프로그램 자체 이름
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port); // 서버와의 연결 설정
    Rio_readinitb(&rio, clientfd); // clientfd를 rio와 연결

    while(Fgets(buf, MAXLINE, stdin) != NULL) { //fgets = 파일에서 읽은 문자열 저장할 메모리 주소, 저장할 문자 최대 개수, 파일 포인터
        Rio_writen(clientfd, buf, strlen(buf)); // buf에서 buf size만큼 clientfd로 전송
        Rio_readlineb(&rio, buf, MAXLINE); // rio에서 읽고 buf로 복사 후 종료(MAXLINE -1개 바이트를 읽어온다)
        Fputs(buf, stdout); // buf를 만든 파일에 집어넣는다(파일에 쓸 문자열, 파일 포인터)
    }
    Close(clientfd); 
    exit(0); // 에러 없을 때 자연종료
}


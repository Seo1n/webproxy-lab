/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

// https://d-cron.tistory.com/37

#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method); // 과제 11.11 
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) { // argc: 인자 개수, argv: 인자 배열
  int listenfd, connfd; // 듣기 identifier
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) { // 입력 인자 2개가 아니라면
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1); // 에러 시 강제 종료
  }

  /*듣기 소켓 오픈, argv는 main 함수가 받은 각각의 인자들
  agrv[1]은 우리가 부여하는 포트 번호, 듣기 식별자 return*/ 

  listenfd = Open_listenfd(argv[1]); // 포트 번호에 연결 요청을 받을 준비가 된 듣기식별자 return

  while (1) {
    /* accept 함수를 호출해서 클라이언트로부터의 연결 요청을 기다림 
    client 소켓은 서버 소켓의 주소를 알고 있으니 client >> server로 넘어올 때 주소 정보를 가지고 올 것이라 가정함
    accept 함수는 연결되면 식별자 connfd를 return */
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 듣기식별자, 소켓 주소 구초체의 주소, 주소(소켓 구조체)
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // clientaddr에 대응되는 hostname, port 작성 
    // >> getnameinfo가 소켓 주소 구조체를 대응되는 호스트와 서비스이름 스트링을 변환한다
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 어떤 클라이언트가 들어왔는지 알려준다
    doit(connfd);   // line:netp:tiny:doit connfd로 트랜잭션 수행
    Close(connfd);  // line:netp:tiny:close connfd로 자신쪽의 연결 끝 닫기
  }
}

// http transaction 처리
void doit(int fd) {  // fd = connfd
  int is_static; // 정적 파일인지 아닌지를 나타냄
  struct stat sbuf; // 파일 정보 저장을 위한 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd); // rio 초기화
  Rio_readlineb(&rio, buf, MAXLINE); // buf에서 client request를 읽어온다
  printf("Request headers: \n"); 
  printf("%s", buf); // 읽어와서 헤더 읽고 출력
  sscanf(buf, "%s %s %s", method, uri, version); // buf에 있는 데이터를 method, uri, version에 담는다

  if (strcasecmp(method, "GET") != 0 || strcasecmp(method, "HEAD") == 0) {// method = GET이 아니면 에러 출력
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return; // main으로 return
  }
  read_requesthdrs(&rio); //요청 헤더 출력

/* URI를 filename과 CGI argument string으로 parse(분석하여 실행할 수 있는 파일로 변경)하고
request가 static인지 dynamic인지 확인하는 flag를 return한다. (1 = static) */

  is_static = parse_uri(uri, filename, cgiargs); // uri를 바탕으로 filename과 cgiargs가 채워짐
  if (stat(filename, &sbuf) < 0) { // disk에 파일이 없다면 filename을 sbuf에 넣는다. 실패시 -1
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }


/*
S_ISREG = 파일 종류 확인 : 일반 파일인지 판별함
S_IRUSR = 읽기 권한을 가지고 있는지 판별함
*/
  if (is_static) { // 정적 파일이 읽기 권한이 없거나 정규 파일이 아니라면 읽을 수 없다
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method); // 정적 파일을 클라이언트에게 제공
  }
  else { // dynamic 실행가능 분별
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method); // 실행 가능 시 동적파일 제공
  }
}

// http 응답을 상태 코드와 메세지로 클라이언트에게 전송
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  // Build the http response body
  // sprintf = 출력 결과값 변수에 저장
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // Print the http response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* Tiny는 request header 정보 사용 x, 
요청 라인 한줄, 요청 헤더를 여러줄 받는데 '요청 라인을 저장해주고' request header는 그냥 출력한다. */

void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); // 한줄 읽어들임

  //같은 경우에 0을 반환하기 때문에 \r\n == buf, 즉 모든 줄을 읽고 마지막 줄에 도착했을 때 break된다.
  while(strcmp(buf, "\r\n")) { // strcmp = str1과 str2를 비교하는데 str1이 str2보다 작으면 0보다 작고 크면 0보다 큰 값 반환. 같으면 0반환 
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf); // 한줄 씩 읽은 것 출력
  }
  return;
}

// 정적 컨텐츠를 위한 home directory = its current directory라고 가정했을 때,
// home directory = /cgi-bin

int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  // strstr = (대상 문자열, 검색할 문자열), cgi-bin이라는 문자열이 없다면 static content
  // strcat = str2를 str1로 연결
  // strcpy = str1에서 지정한 곳으로 str2 복사

  
  if (!strstr(uri, "cgi-bin")) { // if static content
    strcpy(cgiargs, ""); // cgi 인자 string을 지운다
    strcpy(filename, "."); // 상대 리눅스 경로이름으로 변환 '.'
    strcat(filename, uri); // '.' + /index.html

    if (uri[strlen(uri) - 1] == '/') // uri가 /로 끝난다면
      strcat(filename, "home.html"); // homehtml, 즉 기본 파일 추가
    return 1;
  }
  else { // dynamic content, 모든 cgi 인자들 추출
    // index: 첫번째 인자에서 두번째 인자를 찾는다. 찾으면 문자 위치 포인터를 반환하고 못찾으면 NULL 반환함
    ptr = index(uri, '?'); //uri 부분에서 file name과 args를 구분하는 ?위치를 찾는다.
    if (ptr) { // ?가 있을 때
      strcpy(cgiargs, ptr + 1); //cgiargs에 인자를 넣어주고
      *ptr = '\0'; // ptr은 null처리
    }
    else // ?가 없다면 string 지우고 상대 리눅스 경로이름으로 변환한다
      strcpy(cgiargs, "");
    strcpy(filename, "."); 
    strcat(filename, uri);
    return 0;
  }
}

/*
서버가 disk에서 파일 찾음 >> 메모리 영역으로 복사 >> client fd로 복사
*/

void serve_static(int fd, char *filename, int filesize, char *method) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  // Send response headers to client
  get_filetype(filename, filetype); // 5개 중 무슨 파일형식인지 검사해서 filetype 넣기
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf); // while문 돌면 close가 됨. 새로 연결해도 새로 connect하므로 close가 default값.
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize); 
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // \r\n 빈줄하나가 헤더종료표시 
  Rio_writen(fd, buf, strlen(buf)); // buf에서 strlen(buf) 바이트만큼 fd로 전송한다. buf는 가만히 있고 sbuf같은걸 설정 
  printf("Response headers:\n"); 
  printf("%s", buf);

  if (strcasecmp(method, "HEAD") == 0) // head method면 return해서 헤더값만 보여주기
    return;
  // Send response body to client
  // Open(열려고 하는 대상 파일 이름, 파일 열 때 적용되는 열기 옵션, 파일 열 때 접근 권한 설명)
  // O_RDONLY = 읽기 전용으로 파일 열기
  srcfd = Open(filename, O_RDONLY, 0); // filename의 파일을 읽기전용으로 열어서 식별자를 받아온다
 // mmap을 호출하면 파일 srcfd의 첫번째 filesize 바이트를 주소 srcp에서 시작하는 사적 읽기-허용 가상메모리 영역으로 매핑해준다
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = (char*)malloc(filesize); // mmap 대신 malloc 사용 과제 11.9
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd); // 매핑 후 식별자가 더 이상 필요없으므로 메모리 누수 방지 close
  Rio_writen(fd, srcp, filesize); // 주소 srcp에서 filesize만큼 fd로 전송 
  // Munmap(srcp, filesize); //매핑된 가상메모리 주소를 반환(누수 방지)
  free(srcp);

}

// Tiny static content= HTML file, text file, gif, png, jpeg 
void get_filetype(char *filename, char *filetype) { // filename 문자열 안에 html,gif 등 있는지 검사
  if(strstr(filename, ".html")) 
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif")) 
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png")) 
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg")) 
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpg"); // 과제 11.7
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
  char buf[MAXLINE], *emptylist[] = { NULL }; 

  // cleint에게 성공 응답 라인 보내기
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { // 새로운 child process를 fork한다
  // 이때 부모 프로세스는 자식의 PID(process ID)를, 자식 프로세스는 0 반환받음
  // 자식은 QUERY_STRING 환경변수를 요청 URI에 CGI 인자들을 초기화함
    setenv("QUERY_STRING", cgiargs, 1); // 3번째 인자는 기존 환경 변수의 유무에 상관없이 값을 변경하겠다면 1, 아니면 0
    setenv("REQUEST_METHOD", method, 1); 
    // dup2 함수를 통해 표준 출력을 클라이언트와 연계된 연결 식별자로 재지정 
    Dup2(fd, STDOUT_FILENO); // dup2는 STDOUT_FILENO가 이미 오픈되어있다면 닫은 뒤 fd를 stdout에 복사 ?
    Execve(filename, emptylist, environ); // run CGI program
  }
  //부모는 자식이 종료되어 정리되는 것을 기다리기 위해 wait 함수에서 블록된다.
  Wait(NULL); 
}


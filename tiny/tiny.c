/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
/*
http://13.124.53.161/godzilla.jpg 을 불렀을 때 서버 입장에서의 코드 흐름
기본적인 흐름은 클라이언트가 HTTP GET 요청을 보내고 서버가 이를 처리해 이미지를 응답으로 보내는 방식이다.

1. 클라이언트 http GET 요청
* GET /godzilla.jpg HTTP/1.0

2. 서버의 main 함수에서 요청을 수신
* 서버는 무한 루프에서 클라이언트의 연결을 기다리고 있음
* connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
* Accept 함수는 클라이언트와의 연결을 받아들여 새로운 소켓 connfd를 반환, 이 소켓을 통해 서버와 클라이언트 간에 데이터를 주고 받는다

3.doit(connfd)
* 클라이언트 요청 읽기
  * Rio_readlineb 함수는 클라이언트가 보낸 요청 라인을 읽습니다. 이 요청은 GET /godzilla.jpg HTTP/1.0 같은 형식으로 들어옵니다.
* HTTP 메서드와 URL 파싱
  * sscanf 함수는 요청 라인에서 HTTP 메서드(GET), URI(/godzilla.jpg), 그리고 HTTP 버전을 각각 파싱합니다.
* 요청이 GET인지 확인
  * strcasecmp 함수는 메서드가 GET인지 확인, 아니면 에러 메시지 클라이언트 전달

4. URI 분석(parse_uri 함수 호출)
* is_static = parse_uri(uri, filename, cgiargs);
* URI 분석, 요청된 리소스가 정적 콘텐츠인지 동적 콘텐츠인지 판단(cgi-bin이 포함되지 않으면 정적 콘텐츠로 간주)

5. 파일 정보 확인 (stat 함수)
* stat 함수는 filename에 해당하는 파일이 실제로 존재하는지, 그리고 접근할 수 있는지 확인합니다.

6. 정적 콘텐츠 제공(serve_static 함수 호출)
* 이 함수는 클라이언트에게 파일을 전송하는 역할을 한다.

7. 파일 MIME 타입 설정(get_filetype 함수)
* get_filetype 함수는 요청된 파일의 MIME 타입을 설정합니다. 파일 확장자를 확인해 적절한 MIME 타입을 선택합니다.

8. HTTP 응답 헤더 전송
클라이언트에게 HTTP 응답 헤더를 전송합니다. 응답 헤더는 HTTP 상태 코드, 서버 정보, 콘텐츠 길이, MIME 타입을 포함합니다.
sprintf(buf, "HTTP/1.0 200 OK\r\n");
sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
Rio_writen(fd, buf, strlen(buf));

9. 파일 내용을 메모리 매핑 (Mmap 함수)
* 서버는 godzilla.jpg 파일의 내용을 메모리에 매핑합니다. 이를 통해 파일 내용을 직접 읽지 않고도 클라이언트에게 전송할 수 있습니다.
* Mmap을 통해 godzilla.jpg의 내용이 srcp에 매핑됩니다.

10. 파일 내용 전송(Rio_writen 함수)
* 매핑된 파일 내용을 클라이언트에게 전송
* Rio_writen(fd, srcp, filesize);
* godzilla.jpg 파일의 내용을 클라이언트 소켓에 기록하여 전송

11. 메모리 매핑 해제 (Munmap 함수)
* 파일 전송이 끝나면, 메모리에 매핑된 파일을 해제

12. 클라이언트가 이미지 수신 및 렌더링
* 클라이언트는 서버로부터 godzilla.jpg 파일을 받는다.

*/
#include "csapp.h"

void doit(int fd);                                        // 클라이언트 요청을 처리하는 함수
void read_requesthdrs(rio_t *rp);                         // 클라이언트로부터 요청 헤더를 읽는 함수
int parse_uri(char *uri, char *filename, char *cgiargs);  // 요청된 URI를 분석하여 파일이름과 CGI 인자 추출
void serve_static(int fd, char *filename, int filesize, char *method);  // 정적 파일 제공 함수
void get_filetype(char *filename, char *filetype);        // 파일의 MIME 타입을 결정하는 함수
void serve_dynamic(int fd, char *filename, char *cgiargs);// CGI 프로그램을 실행하는 동적 콘텐츠 제공 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg); // 클라이언트에게 에러 메시지를 전송하는 함수

int main(int argc, char **argv) {
  int listenfd, connfd;                   // listen 소켓과 클라이언트와의 연결 소켓을 저장할 변수
  char hostname[MAXLINE], port[MAXLINE];  // 클라이언트의 호스트 이름과 포트 번호를 저장할 버퍼
  socklen_t clientlen;                    // 클라이언트 소켓 주소의 길이를 저장할 변수
  struct sockaddr_storage clientaddr;     // 클라이언트의 소켓 주소를 저장할 구조체 (IPv4, IPv6 모두를 지원)

  // 실행 시 포트 번호가 제대로 전달되지 않으면 사용법을 출력하고 종료
  if (argc != 2) { 
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);}//프로그램 종료

  listenfd = Open_listenfd(argv[1]); // 포트 번호를 사용하여 리슨 소켓을 오픈 (서버가 대기할 준비)

  // 서버는 무한 루프
  while (1) {
    clientlen = sizeof(clientaddr);                               // 클라이언트 주소 구조체의 크기 저장
    connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);    // 클라이언트 연결을 받아들이고 새 소켓 식별자 반환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);// 클라이언트의 호스트 이름과 포트 번호를 얻어옴
    printf("Accepted connection from (%s, %s)\n", hostname, port);// 클라이언트 연결 정보 출력
    doit(connfd);   // 클라이언트 연결 정보 출력
    Close(connfd);  // 클라이언트와의 연결 소켓을 닫음
  }
}
/*doit - client request를 처리해줌*/
void doit(int fd) 
{
    // 콘텐츠가 정적인지 동적인지 구분할 flag 
    int is_static;
    // 파일 정보를 저장할 구조체
    struct stat sbuf;
    // request line에서 메서드, URI, 버전 정보를 저장할 변수들
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    // 파일 이름과 CGI 인자를 저장할 변수
    char filename[MAXLINE], cgiargs[MAXLINE];
    // Robust I/O 버퍼 구조체
    rio_t rio;

    
    Rio_readinitb(&rio, fd);  // fd와 연관된 Robust I/O 읽기 버퍼 초기화
    if (!Rio_readlineb(&rio, buf, MAXLINE)) // 클라이언트로부터 요청 라인을 읽어 buf에 저장
        return;  // 읽을 내용이 없으면 함수 종료
    printf("%s", buf);  // 요청 라인을 출력
    sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인을 파싱하여 메서드, URI, 버전으로 나눔

    // 메서드가 "GET", "HEAD"가 아닌 경우 에러 처리
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {                     
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method"); // 지원되지 않는 메서드에 대한 에러 메시지 전송
        return;
    }

    read_requesthdrs(&rio); // 클라이언트로부터 요청 헤더를 읽음

    /* GET 요청의 URI를 분석하여 정적 콘텐츠인지 동적 콘텐츠인지 구분 */
    is_static = parse_uri(uri, filename, cgiargs); // URI를 분석하여 파일 이름과 CGI 인자를 추출, 정적 콘텐츠이면 is_static에 1 저장
    // 요청한 파일이 존재하는지 확인
    
    if (stat(filename, &sbuf) < 0) {  // 파일 정보를 가져오고 실패 시 에러 처리
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }
    

    if (is_static) { /* 정적 콘텐츠 제공 */
        // 요청한 파일이 읽을 수 있는 정규 파일인지 확인
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, method); // 정적 파일을 제공하는 함수 호출
    }
    else { /* 동적 콘텐츠 제공 */
        // 요청한 파일이 실행 가능한 정규 파일인지 확인
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs); // 동적 콘텐츠를 제공하는 함수 호출
    }
}

/* 클라이언트 에러 메시지를 전송하는 함수 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF]; // 에러 메시지를 저장할 버퍼들

    /* HTTP 응답 본문 생성 */
    sprintf(body, "<html><title>Tiny Error</title>"); // HTML 제목 생성
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); // 본문 배경색 설정
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); // 짧은 에러 메시지 추가
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); // 자세한 에러 메시지 추가
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); // 서버 정보 추가

    /* HTTP 응답 전송 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 상태 라인 생성
    Rio_writen(fd, buf, strlen(buf)); // 상태 라인 전송
    sprintf(buf, "Content-type: text/html\r\n"); // 콘텐츠 타입 명시
    Rio_writen(fd, buf, strlen(buf)); // 전송
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // 콘텐츠 길이 명시
    Rio_writen(fd, buf, strlen(buf)); // 전송
    Rio_writen(fd, body, strlen(body)); // 에러 본문 전송
}

/* 클라이언트로부터 요청 헤더를 읽는 함수 */
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];  // 요청 헤더를 저장할 버퍼
    Rio_readlineb(rp, buf, MAXLINE); // 요청 헤더의 첫 번째 줄 읽기
    printf("%s", buf);  // 읽은 요청 헤더 출력
    while(strcmp(buf, "\r\n")) {  // 빈 줄이 나올 때까지 반복
        Rio_readlineb(rp, buf, MAXLINE);  // 요청 헤더를 계속 읽음
        printf("%s", buf);  // 읽은 요청 헤더 출력
    }
    return;
}

/*parse_uri - parse URI into filename and CGI args return 0 if dynamic content, 1 if static*/
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    // 요청된 URI가 'cgi-bin'을 포함하지 않으면 정적 콘텐츠로 간주
    if (!strstr(uri, "cgi-bin")) {  /* Static content */ 
        strcpy(cgiargs, "");  // CGI 인자를 비워둠, 정적 콘텐츠에는 CGI 인자가 필요 없으므로
        strcpy(filename, ".");  // 파일 경로를 현재 디렉토리로 초기화
        strcat(filename, uri);  // URI를 파일 경로에 추가해서 전체 경로를 만듦
        if (uri[strlen(uri)-1] == '/')  // URI가 '/'로 끝나면 기본 파일로 'home.html'을 추가
            strcat(filename, "home.html"); 
        return 1;  // 1을 반환하여 정적 콘텐츠임을 나타냄
    } 
    else {  /* Dynamic content */  // 'cgi-bin'이 포함된 URI는 동적 콘텐츠로 간주
        ptr = index(uri, '?');  // '?'를 찾아서 CGI 인자와 프로그램 경로를 구분
        if (ptr) {
            strcpy(cgiargs, ptr + 1);  // '?' 뒤의 부분을 CGI 인자로 저장
            *ptr = '\0';  // '?' 앞부분을 URI의 끝으로 설정 (파일 경로만 남도록)
        }
        else {
            strcpy(cgiargs, "");  // CGI 인자가 없으면 빈 문자열로 설정
        }
        strcpy(filename, ".");  // 파일 경로를 현재 디렉토리로 설정
        strcat(filename, uri);  // URI를 경로에 추가
        return 0;  // 0을 반환하여 동적 콘텐츠임을 나타냄
    }
}
/*static content를 client에게 전달*/
void serve_static(int fd, char *filename, int filesize, char *method) 
{
    int srcfd;  // 파일 디스크립터
    char *srcp, filetype[MAXLINE], buf[MAXBUF];  // 파일을 메모리에 매핑할 포인터와 파일 타입, 버퍼

    // 클라이언트에 응답 헤더 전송
    get_filetype(filename, filetype);  // 파일의 MIME 타입을 얻음 (.html, .jpg 등)
    sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 응답 상태 코드 설정 (200 OK)
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);  // 서버 이름 추가
    sprintf(buf, "%sConnection: close\r\n", buf);  // 연결 종료 알림
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);  // 콘텐츠 길이 설정
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);  // 콘텐츠 타입 설정
    Rio_writen(fd, buf, strlen(buf));  // 클라이언트에 응답 헤더를 전송
    printf("Response headers:\n");
    printf("%s", buf);  // 서버에서 응답 헤더를 출력

    // 메소드 헤드 처리
    if (strcasecmp(method, "HEAD") == 0)
      return;

    // 클라이언트에 응답 본문 전송
    srcfd = Open(filename, O_RDONLY, 0);  // 파일을 읽기 전용으로 열어 파일 디스크립터를 얻음
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  // 파일을 메모리에 매핑
    srcp = (char *)malloc(filesize) ;  // malloc 사용, 데이터 할당
    Rio_readn(srcfd,srcp,filesize); // srcfd에서 filesize 바이트만큼의 데이터를 srcp에 읽어옴
    Close(srcfd);  // 파일 디스크립터를 닫음 (파일을 더 이상 사용할 필요 없음)
    Rio_writen(fd, srcp, filesize);  // 메모리에 매핑된 파일 내용을 클라이언트에 전송
    // Munmap(srcp, filesize);  // 파일의 메모리 매핑을 해제
    free(srcp);
}
//MIME type을 읽고 값을 *filetype에 저장
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))  // 파일 확장자가 .html이면
        strcpy(filetype, "text/html");  // MIME 타입을 text/html로 설정
    else if (strstr(filename, ".gif"))  // .gif 파일이면
        strcpy(filetype, "image/gif");  // MIME 타입을 image/gif로 설정
    else if (strstr(filename, ".png"))  // .png 파일이면
        strcpy(filetype, "image/png");  // MIME 타입을 image/png로 설정
    else if (strstr(filename, ".jpg"))  // .jpg 파일이면
        strcpy(filetype, "image/jpeg");  // MIME 타입을 image/jpeg로 설정
    else if (strstr(filename, ".mp4"))  // .mp4 파일이면
        strcpy(filetype, "video/mp4");  // MIME 타입을 video/mp4로 설정
    else
        strcpy(filetype, "text/plain");  // 알 수 없는 확장자일 경우 기본 MIME 타입인 text/plain을 설정
}

/*Dynamic content의 CGI program 실행*/
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };  // 응답을 저장할 버퍼, 빈 인자 리스트

    // HTTP 응답의 첫 부분 (상태 코드와 서버 정보) 전송
    sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 상태 코드 200 OK
    Rio_writen(fd, buf, strlen(buf));  // 클라이언트에 상태 코드를 전송
    sprintf(buf, "Server: Tiny Web Server\r\n");  // 서버 정보를 추가
    Rio_writen(fd, buf, strlen(buf));  // 서버 정보 전송
    // 자식 프로세스를 생성하여 CGI 프로그램을 실행
    if (Fork() == 0) { /* Child */  // 자식 프로세스
        setenv("QUERY_STRING", cgiargs, 1);  // 환경 변수 QUERY_STRING을 설정하여 CGI(15000&213) 인자 전달
        Dup2(fd, STDOUT_FILENO);  /* 표준 출력을 클라이언트로 리디렉션 */
        Execve(filename, emptylist, environ); /* CGI 프로그램 실행 (filename 실행) */
    }
    // 부모 프로세스는 자식 프로세스가 끝날 때까지 대기
    Wait(NULL);  /* 자식 프로세스가 끝날 때까지 부모 프로세스가 기다림 */
}
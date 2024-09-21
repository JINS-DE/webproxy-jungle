#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void server_request_header(uri);

// argc : 명령줄에서 전달된 인수의 개수 예시) ./tiny 8080 이면 argc는 2
// argv : 명령줄에서 전달된 인수의 배열 argv[0]="./tiny" , argv[1]="8080"
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
    int serverfd;
    // request line에서 메서드, URI, 버전 정보를 저장할 변수들
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE],host[MAXLINE],hostname[MAXLINE],path[MAXLINE],port[MAXLINE],server_request[MAXLINE];
    // Robust I/O 버퍼 구조체
    rio_t rio_client, rio_server;

    Rio_readinitb(&rio_client, fd);  // fd와 연관된 Robust I/O 읽기 버퍼 초기화
    if (!Rio_readlineb(&rio_client, buf, MAXLINE)) // 클라이언트로부터 요청 라인을 읽어 buf에 저장
        return;  // 읽을 내용이 없으면 함수 종료

    /* Client 요청 라인 파싱 */
    sscanf(buf, "%s %s %s", method, uri, version); // 공백으로 구분해서 요청 라인을 파싱하여 메서드, URI, 버전 저장
    
    // "http://%[^/]%s" 의 의미 : http://부터~ , %[^/] : '/'나오기 전에 문자 전부, %s : %[^/]이후에 나머지
    sscanf(uri, "http://%[^/]%s", host, path);  // host: localhost:8000, path: ''
    sscanf(host, "%[^:]:%s",hostname,port);     // hostname:localhost, port:8000
    // path가 빈 경우 /로 설정
    if (path[0] == '\0') {strcpy(path, "/"); }// 빈 문자열인 경우 '/'로 설정    
    
    /* Tiny 서버에 연결  */ 
    serverfd = Open_clientfd(hostname, port);
    if (serverfd < 0) {
        fprintf(stderr, "Failed to connect to Tiny server\n");
        return;
    }

    // 요청 헤더 처리
    sprintf(server_request,"%s %s HTTP/1.0\r\n",method,path);
    sprintf(server_request,"%sHost:%s\r\n",server_request,hostname);
    sprintf(server_request,"%sUser-Agent:%s\r\n",server_request,user_agent_hdr);
    sprintf(server_request,"%sConnection: close\r\n",server_request);
    sprintf(server_request,"%sProxy-Connection: close\r\n",server_request);
    // 브라우저의 추가적인 http 요청 처리

    int n;
    while (Rio_readlineb(&rio_client, buf, MAXLINE)){
      if (strstr(buf,"\r\n\r\n")){break;}
      printf("%s",buf);
      // char *tmp;
      // char key;
      // tmp=strchr(buf,':');
      // *tmp='\0';
      // strcpy(key,buf);
      // printf(key);
      
    }
    printf("\n--------------\n");

    
  
    Rio_writen(serverfd, server_request, strlen(server_request));
    // 서버 소켓 닫기
    Close(serverfd);
}
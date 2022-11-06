/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);            // fd에 대해서 입출력 수행할 rio 구조체 생성.
  Rio_readlineb(&rio, buf, MAXLINE);  // 요청 라인 읽어서 buf에 넣어줌.
  printf("Request headers:\n");
  printf("%s", buf);
    
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 띄어쓰기로 구분된 문자열 3개를 읽어서 뒤의 변수에 넣어줌.
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented",
              "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);  // filename, cgiargs에 리턴을 받는 것.
  if (stat(filename, &sbuf) < 0) {    // filename에 해당하는 file 정보를 sbuf에 저장받음.
    clienterror(fd, filename, "404", "Not found",
              "Tiny couldn't find this file");
    return;
  }
  
  /* Serve static content */
  if (is_static) {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {   // file이 regular파일이 아니거나, readable하지 않은 filename일 경우.
      clienterror(fd, filename, "403", "Forbidden",
              "Tiny couldn't read this file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  /* Serve dynamic content */
  else {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {   // file이 regular파일이 아니거나, executable하지 않은 filename일 경우.
      clienterror(fd, filename, "403", "Forbidden",
              "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);    
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

  /* Print the HTTP response */
  /* response headers */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  /* response body */
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {  // 0이 아닌 값(true)이 나올 때. 즉, readline했을 때 값이 "\r\n"가 아닐 때.
    Rio_readlineb(rp, buf, MAXLINE);  // 읽고서,
    printf("%s", buf);  // 그냥 서버측 표춘 출력으로 출력해버림
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{          // uri는 request line을 읽으면서 이미 얻었음.
  char *ptr;

  if(!strstr(uri, "cgi-bin")) {   /* Static content */
    strcpy(cgiargs, "");    // 정적 컨텐츠 요청이므로, doit에 있는 cgiargs 저장버퍼는 지운다.
    strcpy(filename, "."); strcat(filename, uri);  // convert the URI into a relative Linux pathname such as "./some/whatever.html"
    if (uri[strlen(uri)-1] == '/')  // uri 끝에 파일이름이 명시 안되어 있다면,
      strcat(filename,  "home.html");
    return 1; // is_static = 1
  }
  else {  /* Dynamic content */
    /* 인자 획득 하기 */
    ptr = index(uri, '?');
    if(ptr) {  // uri에 '?'가 있었다면. 즉 동적 컨텐츠에 대한 인자가 있었다면.
      strcpy(cgiargs, ptr+1);   // doit에 있는 cgiargs 저장버퍼에 저장해둔다.
      *ptr = '\0';
    }
    else {  // 인자 없었다면,
      strcpy(cgiargs, "");  // doit에 있는 cgiargs 저장버퍼는 지운다.
    }
    /* 파일 경로 만들기 */
    strcpy(filename, "."); strcat(filename, uri); // convert the URI into a relative Linux pathname such as "./some/whatever.html"
    return 0; // is_static = 0
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;  // source file descriptor
  char *srcp; // source pointer
  char filetype[MAXLINE], buf[MAXLINE];

  /* Send response headers to client*/
  get_filetype(filename, filetype);  //filename : "./some/whatever.html"
  
  // make request line
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  // make request headers
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);    // use of filetype
  // send
  Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);  // 디스크 파일 연다.
  // make request body
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 디스크에 있는 file을 Mmap으로 메모리에 올려놓고, (일종의 버퍼라고 생각해도 되겠다)
  Close(srcfd); // 디스크파일은 닫고,
  // send
  Rio_writen(fd, srcp, filesize); // connfd에 (메모리에 올려진) 디스크파일 내용 적는다. 즉, client에게 파일 내용을 보낸다.
  Munmap(srcp, filesize); // 마친후, 디스크파일 올려놨던 메모리 공간도 반환.
}

/* available file type : HTML, text, GIF, PNG, JPEG */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE];
  char *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  // make and send response line
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  // make and send part of response headers
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);  // Dup2(int oldfd, int newfd): creates a copy of the file descriptor oldfd to file descriptor newfd.
                              // If the file descriptor newfd was previously open, it is silently closed before being reused.
                              // 한국말: newfd 식별자가 oldfd 식별자가 가리키는 open file을 가리키게 한다.
                              // newfd 식별자가 원래 어떤 open file을 가리키고 있었다면,
                              // 가리키던 open file의 참조 횟수를 1 감소시킨다. (어떤 open file의 참조회수가 0이 되면 커널은 그 파일을 닫는다.)
                              // * 우리 코드 에서*
                              // 기억할 것: 자식 프로세스는 부모 프로세스의 식별자 테이블을 그대로 복사해서 자신만의 식별자 테이블을 갖는다.
                              // 그래서, 자식 프로세스의 식별자 테이블을 부모 프로세스의 식별자 테이블이 가리키는 오픈 파일들을 동일하게 가리키고 있을 것이다.
                              // 자식 프로세스의 식별자 테이블에서 식별자 1(표준출력)은 부모의 식별자 테이블에서와 마찬가지로 '터미널(쉘)'이라는 '파일'을 가리키고 있을 것이다.
                              // dup2(connfd, STDOUT_FILENO) 를 통해 자식 프로세스의 식별자 테이블에서 식별자 1은, 자신의 식별자 테이블에서 식별자 connfd가 가리키던 오픈 파일을 가리키게 된다. 부모 프로세스의 식별자 테이블에서 connfd가 가리키던 오픈 파일은 연결소켓이었으므로, 자식 프로세스의 식별자 테이블에서의 connfd가 가리키던 오픈 파일 또한 동일할 것이다.
                              // (부모 프로세스의 식별자 테이블에서 식별자 1은 여전히 '터미널(쉘)'이라는 '파일'을 가리키고 있다.))
                              // 결국, 자식 프로세스가 '표준출력'으로 뭔가를 쓰게 된다면 소켓에 쓰는 것과 마찬가지가 되는 것이다.
    Execve(filename, emptylist, environ);
  }
  Wait(NULL); /* Parent waits for and reaps child */
}
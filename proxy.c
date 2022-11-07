#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

void doit(int fd);
void forward_request(int clientfd, char *method, char *filename, char *host);
void serve_response(rio_t *rp, int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
int parse_uri(char *uri, char *host, char *port, char *filename);
void read_requesthdrs(rio_t *rp);

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
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) {
  int clientfd;
  char buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE], filename[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);            // 새로운 rio (connfd).
  Rio_readlineb(&rio, buf, MAXLINE);  // 첫번째 줄(request line) 읽어서 buf에 넣어줌.
  printf("Incoming Request headers:\n");
  printf("%s\n", buf);
    
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 띄어쓰기로 구분된 문자열 3개를 읽어서 뒤의 변수에 넣어줌.
  // if (strcasecmp(method, "GET")) {
  //   clienterror(fd, method, "501", "Not implemented",
  //             "Tiny does not implement this method");
  //   return;
  // }
  
  read_requesthdrs(&rio); // valid check, find addintional header
  /* end of Read request line and headers */


  /* make request to server */
  parse_uri(uri, host, port, filename);
  clientfd = Open_clientfd(host, port);
  forward_request(clientfd, method, filename, host);
  /* end of request to server */

  /* redirect response to client */
  Rio_readinitb(&rio, clientfd);  // 새로운 rio (clientfd).
  serve_response(&rio, fd);
  Close(clientfd);
   /* end of redirect response to client */
}


void forward_request(int clientfd, char *method, char *filename, char *host)
{ 
  char buf[MAXLINE];
  // make request line
  sprintf(buf, "%s %s HTTP/1.0\r\n", method, filename);
  // make request headers
  sprintf(buf, "%sHost: %s\r\n", buf, host);
  sprintf(buf, "%s%s\r\n", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);
  // send
  Rio_writen(clientfd, buf, strlen(buf));
    printf("Request headers to server:\n");
    printf("%s", buf);
}
void serve_response(rio_t *rp, int fd)
{                       // rio has clientfd.
  char *srcp; // source pointer
  size_t src_size = 0;
  size_t res_size = 0;
  char buf[MAXLINE], headers[MAXLINE];

  printf("Response headers from server:\n");
  Rio_readlineb(rp, buf, MAXLINE);    // response line
    Rio_writen(fd, buf, strlen(buf));
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {  // 0이 아닌 값(true)이 나올 때. 즉, readline했을 때 값이 "\r\n"가 아닐 때.
    Rio_readlineb(rp, buf, MAXLINE);
    if (strstr(buf, "Content-length")){
      src_size = atoi(rindex(buf, ':')+2);
    }
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);
  }
  if (src_size) {
    // lseek(rp->rio_fd, 0, SEEK_SET);
    // res_size = strlen(headers)+src_size;
    srcp = malloc(src_size);
    Rio_readnb(rp, srcp, src_size);
    Rio_writen(fd, srcp, src_size);
    //printf("--below is sent to client--\n");
    //printf(srcp);
    free(srcp);
    //Munmap(srcp, src_size);
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

int parse_uri(char *uri, char *host, char *port, char *filename)
{
  char *host_ptr = index(uri, ':')+3;
  char *port_ptr = index(host_ptr, ':')+1;
  char *filename_ptr = index(port_ptr, '/');
  strncpy(host, host_ptr, port_ptr-host_ptr-1);
  strncpy(port, port_ptr, filename_ptr-port_ptr);
  strcpy(filename, filename_ptr);

  // valid check(?)
  return 1;
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {  // 0이 아닌 값(true)이 나올 때. 즉, readline했을 때 값이 "\r\n"가 아닐 때.
    Rio_readlineb(rp, buf, MAXLINE);  // 읽고서,
    printf("%s", buf);  // 그냥 서버측 표춘 출력으로 출력해버림
  }
  return;
}
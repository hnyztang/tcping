#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAXBUF 1024

int main(int argc, char **argv)
{
  int sockfd, new_fd;
  socklen_t len;
  struct sockaddr_in my_addr, their_addr;
  unsigned int myport, backlog;
  char buf[MAXBUF + 1];
  if (argc != 2) {
    printf ("Usage: %s port", argv[0]);
    exit(0);
  }
  myport = atoi(argv[1]);
  backlog = atoi("10000"); // listening connections
  my_addr.sin_addr.s_addr = inet_addr("0"); // listening address
  if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket error");
    exit(1);
  }
  bzero(&my_addr, sizeof(my_addr));
  my_addr.sin_family = PF_INET;
  my_addr.sin_port = htons(myport);
  if (bind(sockfd, (const struct sockaddr*) &my_addr, sizeof(struct sockaddr)) == -1) {
    perror("bind error");
    exit(2);
  }
  if (listen(sockfd, backlog) == -1) {
    perror("listen error");
    exit(3);
  }
  while(1) {
    len = sizeof(struct sockaddr);
    if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &len)) == -1) {
      perror("accept error");
      exit(errno);
    }
    else {
      printf("connection from address:'%s', port:'%d', socket:'%d'\n",
        inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port), new_fd);
    }
    bzero(buf, MAXBUF + 1);
    strcpy(buf, inet_ntoa(their_addr.sin_addr));
    len = send(new_fd, buf, strlen(buf), 0);
    if (len > 0) {
      printf("msg send success, bytes:'%d', msg:'%s'\n", len, buf);
    }
    else {
      printf("msg send failed:'%s', errno:'%d', error:'%s'\n", buf, errno, strerror(errno));
    }
    bzero(buf, MAXBUF + 1);
    len = recv(new_fd, buf, MAXBUF, 0);
    if (len > 0)
      printf("msg recv success, bytes:'%d', msg:'%s'\n", len, buf);
    else
      printf("msg recv failed. errno:'%d', error:'%s'", errno, strerror(errno));
  }
  close(sockfd);
  return 0;
}

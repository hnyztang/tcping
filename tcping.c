#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAXBUF 1024

int main(int argc, char **argv)
{
  int sockfd, len;
  struct sockaddr_in dest;
  char buffer[MAXBUF + 1];

  if (argc != 3) {
    printf("Usage: %s ip port", argv[0]);
    exit(1);
  }

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket error");
    exit(errno);
  }

  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(atoi(argv[2]));

  if (inet_aton(argv[1], (struct in_addr *) &dest.sin_addr.s_addr) == 0) {
    perror(argv[1]);
    exit(errno);
  }

  if (connect(sockfd, (struct sockaddr *) &dest, sizeof(dest)) != 0) {
    perror("connect error");
    exit(errno);
  }

  bzero(buffer, MAXBUF + 1);
  len = recv(sockfd, buffer, MAXBUF, 0);
  if (len > 0) {
    printf("msg recv success, bytes:'%d', msg:'%s'\n", len, buffer);
  }
  else {
    printf("msg recv failed, errno:'%d', error info: '%s'\n", errno, strerror(errno));
  }

  bzero(buffer, MAXBUF + 1);
  strcpy(buffer, "msg from client");
  len = send(sockfd, buffer, strlen(buffer), 0);
  if (len > 0)
    printf("msg send success, bytes:'%d', msg:'%s'\n", len, buffer);
  else
    printf("msg send failed, errno:'%d', error info:'%s', msg:'%s'\n", errno, strerror(errno), buffer);

  close(sockfd);

  return 0;
}

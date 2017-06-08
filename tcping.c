#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdbool.h>

#define MAXBUF 1024

char* dest_host;
char* dest_port;

long ntransmitted;
long nreceived;
int exiting = 0;
struct timeval start_time, current_time;
long tmin = LONG_MAX, tmax = 0; // rtt: round trip time
long long tsum;

void usage();

void sigexit(int signo) {
  exiting = 1;
}
void finish();
/*
 * Substract 2 timeval structs: out = out-in.
 * Assume out >= in.
 */
void tvsub(struct timeval *out, struct timeval *in) {
  if ((out->tv_usec -= in->tv_usec) < 0) {
    --out->tv_sec;
    out->tv_usec += 1000000;
  }
  out->tv_sec -= in->tv_sec;
}

int gather_statistics(bool isReceived, struct timeval slice_time, struct timeval *gap_time) {
  gettimeofday(&current_time, NULL);
  struct timeval tv = current_time;

  if (isReceived) {
    nreceived++;
    tvsub(&tv, &slice_time);
    long triptime = tv.tv_sec * 1000000 + tv.tv_usec;
    tsum += triptime;
    if (triptime < tmin)
      tmin = triptime;
    if (triptime > tmax)
      tmax = triptime;

    *gap_time = tv;
  }

  usleep(1000000);
  ntransmitted++;
  return 0;
}

void finish(void) {
  struct timeval tv = current_time;
  tvsub(&tv, &start_time);

  putchar('\n');
  fflush(stdout);
  printf("--- %s tcping statistics ---\n", dest_host);
  printf("%ld packets transmitted, ", ntransmitted);
  printf("%ld received, ", nreceived);
  printf("%d%% packet loss, ", (int) ((((long long)(ntransmitted - nreceived)) * 100) / ntransmitted));
  printf("time %ldms", tv.tv_sec*1000 + tv.tv_usec/1000);
  putchar('\n');

  if (nreceived) {
    tsum = tsum / nreceived;
    printf("rtt min/avg/max = %ld.%03ld/%lu.%03ld/%ld.%03ld ms",
           (long)tmin/1000, (long)tmin%1000,
           (unsigned long)(tsum/1000), (long)(tsum%1000),
           (long)tmax/1000, (long)tmax%1000
           );
  }
}

int main(int argc, char **argv)
{
  int sockfd, len;
  struct sockaddr_in destaddr;
  int port = 0;
  char buffer[MAXBUF + 1];
  int c;
  struct hostent * host;
  char* endptr;
  int ret;
  fd_set fdrset, fdwset;
  struct timeval timeout;
  socklen_t errlen;
  int error = 0;

  struct timeval slice_time;
  struct timeval gap_time;

  if (argc != 3) {
    usage(argv[0]);
    exit(-1);
  }

  while((c = getopt(argc, argv, "w:h:")) != -1) {
    switch(c) {
      case 'w':
        break;
      case 'h':
        usage(argv[0]);
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  signal(SIGINT, sigexit);
  dest_host = argv[optind];
  dest_port = argv[optind+1];

  if ((host = gethostbyname(dest_host)) == NULL) {
    fprintf(stderr, "tcping: unknown host %s\n", dest_host);
    exit(2);
  }

  memset(&destaddr, 0, sizeof(destaddr));
  memcpy(&destaddr.sin_addr, host->h_addr_list[0], host->h_length);

  if (dest_port) {
    endptr = NULL;
    port = strtol(dest_port, &endptr, 10);
    if (endptr == dest_port) {
      usage(argv[0]);
      exit(-1);
    }
  } else {
    usage(argv[0]);
    exit(-1);
  }

  if (port == 0) {
    fprintf(stderr, "port range %d not valid\n", port);
    usage(argv[0]);
    exit(-1);
  }

  destaddr.sin_port = htons(port);

  fprintf(stdout, "TCPING %s (%s)\n", dest_host, inet_ntoa(destaddr.sin_addr));
  gettimeofday(&start_time, NULL);

for (;;)
{
  if (exiting)
    break;

  gettimeofday(&slice_time, NULL);

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket error");
    exit(errno);
  }
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  if ((ret = connect(sockfd, (struct sockaddr *) &destaddr, sizeof(destaddr))) != 0) {
    if (errno != EINPROGRESS) {
      close(sockfd);
      fprintf(stderr, "%s:%d error connect: %s\n", dest_host, port, strerror(errno));
      gather_statistics(false, slice_time, &gap_time);
      continue;
    }
  }
  FD_ZERO(&fdrset);
  FD_SET(sockfd, &fdrset);
  fdwset = fdrset;

  timeout.tv_sec = 10;
  timeout.tv_usec = 0;

  ret = select(sockfd+1, &fdrset, &fdwset, NULL, &timeout);
  switch (ret) {
    case 0:
      close(sockfd);
      fprintf(stderr, "%s:%d tcp_seq=%ld connect timeout\n", dest_host, port, ntransmitted);
      gather_statistics(false, slice_time, &gap_time);
      continue;
    case -1:
      close(sockfd);
      fprintf(stderr, "error select: %s\n", strerror(errno));
      gather_statistics(false, slice_time, &gap_time);
      continue;
    default:
      if (FD_ISSET(sockfd, &fdrset) || FD_ISSET(sockfd, &fdwset)) {
        errlen = sizeof(error);
        if ((ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &errlen)) != 0) {
          fprintf(stderr, ": %s:%d tcp_seq=%ld error getsockopt: %s\n", dest_host, port, ntransmitted, strerror(errno));
          close(sockfd);
          gather_statistics(false, slice_time, &gap_time);
          continue;
        }
        if (error != 0) {
          fprintf(stderr, "%s:%d tcp_seq=%ld connect failed\n", dest_host, port, ntransmitted);
          close(sockfd);
          gather_statistics(false, slice_time, &gap_time);
          continue;
        }
      } else {
        fprintf(stderr, "%s:%d tcp_seq=%ld sockfd not set\n", dest_host, port, ntransmitted);
        gather_statistics(false, slice_time, &gap_time);
        continue;
      }
  }

  close(sockfd);

  gather_statistics(true, slice_time, &gap_time);
  fprintf(stdout, "%s:%d tcp_seq=%ld time=%ld.%03dms\n", inet_ntoa(destaddr.sin_addr), port, ntransmitted,
           gap_time.tv_sec*1000+gap_time.tv_usec/1000, gap_time.tv_usec%1000);

}
  finish();
  exit(0);
}

void usage(char* prog) {
  fprintf(stderr, "Usage: %s <host> <port>\n", prog);
}

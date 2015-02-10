#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXLINE 4096
#define err_sys(errmsg)	\
	do { perror(errmsg); exit(EXIT_FAILURE); } while (0)

static ssize_t writen(int fd, const void *buf, size_t nbytes);
static ssize_t readn(int fd, void *buf, size_t nbytes);

int main(int argc, char **argv)
{
	ssize_t n;
	int sockfd;
	char recvline[MAXLINE + 1];
	FILE *fp;
	
	struct sockaddr_in servaddr;
	const char *ipaddr = "127.0.0.1";

	fp = fopen("Makefile", "r");

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		perror("socket error");

	memset(&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(1024);	/* daytime server */
	inet_pton(AF_INET, ipaddr, &servaddr.sin_addr);
	
	if (connect(sockfd, (struct sockaddr *) &servaddr,
		    sizeof(servaddr)) == -1) {
		
		perror("connect error");
		exit(EXIT_FAILURE);
	}
	signal(SIGPIPE, SIG_DFL);

	n = read(sockfd, recvline, sizeof(recvline));
	recvline[n] = '\0';	/* null terminate */
	printf("from [server #]: ");
	fputs(recvline, stdout);

	memset(recvline, 0, sizeof(recvline));
	while (fgets(recvline, sizeof(recvline), fp) != NULL) {
		printf("write to serv: ");
		writen(sockfd, recvline, strlen(recvline));
		printf("read from serv: ");
		n = readn(sockfd, recvline, sizeof(recvline));
		recvline[n] = '\0';
		fputs(recvline, stdout);
		memset(recvline, 0, sizeof(recvline));
	}

	close(sockfd);
	return 0;
}

static ssize_t writen(int fd, const void *buf, size_t nbytes)
{
	size_t nleft;
	ssize_t nwriten;
	const char *p;

	nleft = nbytes;
	p = buf;
	while ((nwriten = write(fd, p, nleft)) != 0) {
		if (nwriten == -1) {
			if (errno == EINTR)
				continue;
			perror("write");
			exit(EXIT_FAILURE);
		}
		nleft -= nwriten;
		p += nwriten;
	}
	return nbytes;
}

/* This function is not thread-safe */
static ssize_t readn(int fd, void *buf, size_t nbytes)
{
	size_t nleft;
	ssize_t nread;
	char *p;

	nleft = nbytes;
	p = buf;
	while (nleft > 0) {
		nread = read(fd, p, nleft);
		if (nread == -1) {
			if (errno == EINTR)
				nread = 0;
			else
				err_sys("read error");
		} else if (nread == 0)
			break;
		nleft -= nread;
		p += nread;
	}
	return nbytes - nleft;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

/* The best buffer size is greater than or equal to 4096 in linux-x86_64 */
#define MAXLINE 4096
#define err_sys(string) \
	do { perror(string); exit(EXIT_FAILIURE); } while (0)

static void err_quit(const char *msg, ...);

int main(int argc, char **argv)
{
	int sockfd, n, counter;
	char recvline[MAXLINE + 1];
	struct sockaddr_in servaddr;
	socklen_t addrlen;

	if (argc != 2)
		err_quit("usage: a.out <IPaddress>");

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err_sys("socket error");

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(9999);	/* daytime server */
	if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
		err_quit("inet_pton error for %s", argv[1]);

	addrlen = sizeof(struct sockaddr);
	if (connect(sockfd, (struct sockaddr *) & servaddr, addrlen) == -1)
		err_sys("connect error");

	counter = 0;
	while ((n = read(sockfd, recvline, MAXLINE)) > 0) {
		counter++;
		recvline[n] = '\0';	/* null terminate */
		if (fputs(recvline, stdout) == EOF)
			err_sys("fputs error");
	}
	if (n < 0)
		err_sys("read error");

	printf("counter = %d\n", counter);
	close(sockfd);
	exit(0);
}

static void err_quit(const char *format, ...)
{
	va_list ap;
	char buf[MAXLINE];

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	fputs(buf, stderr);
	va_end(ap);
	exit(EXIT_FAILIURE);
}

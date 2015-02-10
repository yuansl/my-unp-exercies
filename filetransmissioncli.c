/* echo-service server */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 4096

static void err_exit(const char *msg);
static void do_talk(int sockfd);

int main(void)
{
	int sockfd;
	struct sockaddr_in addr;
	const char *ip = "127.0.0.1";
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err_exit("socket");

	addr.sin_family = AF_INET;
	addr.sin_port = htons(1024);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) == -1)
		err_exit("inet_pton");
	if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		err_exit("connect");
	do_talk(sockfd);
	close(sockfd);
	return 0;
}

static void err_exit(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}
			/* Read "n" bytes from a descriptor. */
static void do_talk(int sockfd)
{
	char buf[MAXLINE];
	ssize_t n;

	freopen("S08E05.mkv", "r", stdin);
	FILE *fp = fopen("/home/yuansl/Downloads/b.mkv", "w");

	if (fp == NULL) {
		fprintf(stderr, "fopen error: %s\n", strerror(errno));
		fp = freopen("/dev/tty", "w+", stdout);
	}
	while (true) {
		if ((n = read(fileno(stdin), buf, sizeof(buf))) == -1)
			err_exit("read error");
		if (n == 0)
			break;
		if (write(sockfd, buf, n) != n)
			err_exit("write error");
		if ((n = read(sockfd, buf, sizeof(buf))) == -1)
			err_exit("read error");
		if (write(fileno(fp), buf, n) == -1)
			err_exit("write error");
	}
	printf("Done\n");
}

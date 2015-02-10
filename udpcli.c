#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

static void err_exit(const char *msg);
static void str_cli(FILE *, int, struct sockaddr_in *, socklen_t);

int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;
	const char *ip = "127.0.0.1";

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err_exit("socket error");

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(1024);
	if (inet_pton(AF_INET, ip, &servaddr.sin_addr) == -1)
		err_exit("inet_pton error");
/*
	if (connect(sockfd,
		    (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
		err_exit("connect error");
*/
	str_cli(stdin, sockfd, &servaddr, sizeof(servaddr));	/* do it all */

	return 0;
}

static void err_exit(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void str_cli(FILE *fp, int sockfd, struct sockaddr_in *addr,
		    socklen_t len)
{
	fd_set rset;
	int maxfdp1;
	ssize_t n;
	char buf[BUFSIZ];

	FD_ZERO(&rset);
	for (;;) {
		FD_SET(sockfd, &rset);
		FD_SET(fileno(fp), &rset);
		maxfdp1 = max(fileno(fp), sockfd) + 1;
		if (select(maxfdp1, &rset, NULL, NULL, NULL) == -1)
			err_exit("select error");

		if (FD_ISSET(sockfd, &rset)) {
			if ((n = recvfrom(sockfd, buf, sizeof(buf), 0,
					  NULL, NULL)) == -1)
				err_exit("recvfrom error");
			buf[n] = '\0';
			fputs(buf, stdout);
		}
		
		if (FD_ISSET(fileno(fp), &rset)) {
			if (fgets(buf, sizeof(buf), fp) == NULL) {
				printf("Done\n");
				return;
			}

			if (sendto(sockfd, buf, strlen(buf), 0,
				   (struct sockaddr *) addr, len) == -1)
				err_exit("sendto error");
		}
	}
}

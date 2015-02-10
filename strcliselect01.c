#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 4096
#define max(a, b) ((a) > (b) ? (a) : (b))

static void err_quit(const char *msg);
static void str_cli(FILE *fp, int sockfd);

int main(void)
{
	struct sockaddr_in servaddr;
	int sockfd;
	const char *ip = "127.0.0.1";

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err_quit("socket");
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(1024);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

	connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	
	str_cli(stdin, sockfd);
	
	close(sockfd);
	return 0;
}

void str_cli(FILE *fp, int sockfd)
{
	int maxfdp1;
	fd_set rset;
	char sendline[MAXLINE], recvline[MAXLINE];
	ssize_t n;
	
	FD_ZERO(&rset);
	for (;;) {
		FD_SET(fileno(fp), &rset);
		FD_SET(sockfd, &rset);
		maxfdp1 = max(fileno(fp), sockfd) + 1;
		
		select(maxfdp1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(sockfd, &rset)) {	/* socket is readable */
			if ((n = read(sockfd, recvline, MAXLINE)) == -1) {
				perror("read error");
				exit(EXIT_FAILURE);
			} else if (n == 0)
				err_quit("str_cli: server terminated "
				       "prematurely\n");
			recvline[n] = '\0';
			fputs(recvline, stdout);
		}

		if (FD_ISSET(fileno(fp), &rset)) {	/* input is readable */
			if (fgets(sendline, MAXLINE, fp) == NULL) {
				printf("End of file, All done.\n");
				return;	/* all done */
			}

			if (write(sockfd, sendline, strlen(sendline)) == -1)
				err_quit("write error");
		}
	}
}

static void err_quit(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

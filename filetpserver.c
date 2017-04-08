#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 4096

static void err_exit(const char *msg);
static void do_talk(int connfd);
static void sig_chld(int signo);

int main(void)
{
	int sockfd, connfd;
	struct sockaddr_in addr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err_exit("socket");

	addr.sin_family = AF_INET;
	addr.sin_port = htons(1024);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		err_exit("bind");

	if (listen(sockfd, 5) == -1)
		err_exit("listen");
	signal(SIGCHLD, sig_chld);
	for (;;) {
		if ((connfd = accept(sockfd, NULL, 0)) == -1)
			err_exit("accept");

		if (fork() == 0) {
			close(sockfd);

			do_talk(connfd);
			exit(EXIT_SUCCESS);
		}
		close(connfd);
	}
	return 0;
}

static void err_exit(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void do_talk(int connfd)
{
	ssize_t n;
	char buf[MAXLINE];
	
	while ((n = read(connfd, buf, sizeof(buf))) != 0) {
		if (n == -1)
			break;
		if (write(connfd, buf, n) == -1)
			err_exit("write");
	}
}

static void sig_chld(int signo)
{
	while (waitpid(-1, NULL, WNOHANG) != -1)
		;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>		/* for gettimeofday */
#include <time.h>		/* for ctime */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define MAXLINE BUFSIZ

static void err_sys(const char *msg);
static char *gf_time(void);

static void str_cli(int sockfd);

int main(int argc, char *argv[])
{
	struct sockaddr_in srvaddr;
	int connfd;

	if (argc < 2) {
		fprintf(stderr, "usage: a.out [ip]\n");
		exit(EXIT_FAILURE);
	}
	connfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&srvaddr, 0, sizeof(srvaddr));
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_port = htons(1024);
	inet_pton(AF_INET, argv[1], &srvaddr.sin_addr);
	if (connfd == -1)
		err_sys("socket error");
	if (connect(connfd, (struct sockaddr *) &srvaddr, sizeof(srvaddr))
	    == -1)
		err_sys("connect error");
	str_cli(connfd);
	return 0;
}

static void str_cli(int sockfd)
{
	ssize_t n;
	int nready, maxfdp1, val;
	fd_set rset, wset;
	bool stdineof;
	char to[BUFSIZ], from[BUFSIZ];
	char *toiptr, *tooptr, *friptr, *froptr;

	val = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, val | O_NONBLOCK);
	val = fcntl(STDOUT_FILENO, F_GETFL, 0);
	fcntl(STDOUT_FILENO, F_SETFL, val | O_NONBLOCK);
	val = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, val | O_NONBLOCK);

	toiptr = tooptr = to;
	friptr = froptr = from;
	stdineof = false;
	maxfdp1 = max(max(STDIN_FILENO, STDOUT_FILENO), sockfd) + 1;
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	/* Note: STDOUT_FILENO and any successful socket is ready 
	 * for write */
	for (;;) {
		if (!stdineof && toiptr < &to[BUFSIZ])
			FD_SET(STDIN_FILENO, &rset);
		if (froptr < friptr)
			FD_SET(STDOUT_FILENO, &wset);
		if (friptr < &from[BUFSIZ])
			FD_SET(sockfd, &rset);
		if (tooptr < toiptr)
			FD_SET(sockfd, &wset);
		nready = select(maxfdp1, &rset, &wset, NULL, NULL);
		if (nready <= 0) {
			if (errno == EINTR)
				fprintf(stderr, "signal interrupt\n");
			else 
				err_sys("select error");
			continue;
		}
	
		if (FD_ISSET(STDIN_FILENO, &rset)) {
			n = read(STDIN_FILENO, toiptr,
				 &to[BUFSIZ] - toiptr);
			if (n == -1) {
				if (errno != EWOULDBLOCK)
					err_sys("read error");
			} else if (n == 0) {
				stdineof = true;
				if (tooptr == toiptr)
					shutdown(sockfd, SHUT_WR);
			} else {
				toiptr += n;
				FD_SET(sockfd, &wset);
			}
		}
		if (FD_ISSET(sockfd, &rset)) {
			n = read(sockfd, friptr, &from[BUFSIZ] - friptr);
			if (n == -1) {
				if (errno != EWOULDBLOCK)
					err_sys("read error");
			} else if (n == 0) {
				if (stdineof)
					shutdown(sockfd, SHUT_RD);
				else
					fprintf(stderr, "\
%s: server terminated prematurely\n", gf_time());
				return;
			}
			friptr += n;
			FD_SET(STDOUT_FILENO, &wset);
		}
		if (FD_ISSET(STDOUT_FILENO, &wset)
		    && (n = friptr - froptr) > 0) {
			n = write(STDOUT_FILENO, froptr, n);
			if (n == -1) {
				if (errno != EWOULDBLOCK)
					err_sys("write error");
			}
			froptr += n;
			if (froptr == friptr) {
				froptr = friptr = from;
				FD_CLR(STDOUT_FILENO, &wset);
			}
		}
		if (FD_ISSET(sockfd, &wset) && (n = toiptr - tooptr) > 0) {
			n = write(sockfd, tooptr, n);
			if (n == -1) {
				if (errno != EWOULDBLOCK)
					err_sys("write error");
			} 
			tooptr += n;
			if (tooptr == toiptr) {
				tooptr = toiptr = to;
				FD_CLR(sockfd, &wset);
			}
			if (stdineof)
				shutdown(sockfd, SHUT_WR);
		}
	}
}

static void err_sys(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static char *gf_time(void)
{
	struct timeval tv;
	static char str[100];
	char *ptr;

	gettimeofday(&tv, NULL);
	ptr = ctime(&tv.tv_sec);
	strcpy(str, &ptr[11]);
	snprintf(str + 8, sizeof(str) - 8, "%.06ld", tv.tv_usec);
	return str;
}

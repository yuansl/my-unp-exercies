#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>             /* for select() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void err_exit(const char *msg);
static void err_quit(const char *msg);
static void str_echo(int listenfd);

int main(void)
{
	int listenfd;
	struct sockaddr_in servaddr;

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(1024);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err_exit("socket error");
	if (bind(listenfd, (struct sockaddr *) &servaddr,
		 sizeof(servaddr)) == -1)
		err_exit("bind error");

	if (listen(listenfd, 5) == -1)
		err_exit("listen error");
	str_echo(listenfd);
	
	return 0;
}

static void err_exit(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void err_quit(const char *msg)
{
	fprintf(stderr, "Error: %s\n", msg);
	exit(EXIT_FAILURE);
}

static void str_echo(int listenfd)
{
	int maxfd, nready, connfd, sockfd, i, maxi, clients[FD_SETSIZE];
	char buf[BUFSIZ];
	ssize_t n;
	fd_set rset, rrset;

	maxfd = listenfd;
	maxi = -1;
	memset(clients, -1, sizeof(clients));
	FD_ZERO(&rrset);
	for (;;) {
		FD_SET(listenfd, &rset);		
		rset = rrset;
		if ((nready = select(maxfd + 1, &rset, NULL, NULL, NULL)) == -1)
			err_exit("select error");

		if (FD_ISSET(listenfd, &rset)) {
			printf("An new connection from client socket\n");
			connfd = accept(listenfd, NULL, NULL);
			for (i = 0; i < FD_SETSIZE; i++)
				if (clients[i] == -1) {
					clients[i] = connfd;
					break;
				}
			if (i == FD_SETSIZE) 
				err_quit("Error: too many clients");
			
			FD_SET(connfd, &rrset);
			if (connfd > maxfd)
				maxfd = connfd;
			if (i > maxi)
				maxi = i;
			if (--nready <= 0)     /* no more readable descriptor */
				continue;
		}

		/* readable descriptors of clients: check all clients for data */
		for (i = 0; i <= maxi; i++) {
			if (clients[i] == -1)
				continue;
			sockfd = clients[i];
			if (FD_ISSET(sockfd, &rset)) {
				n = read(sockfd, buf, sizeof(buf));
				if (n == -1 || n == 0) {
					if (n == -1 && errno == ECONNRESET) {
						fprintf(stderr, "connection "
							"reset by peer\n");
					} else if (n == -1) {
						err_exit("read error");
					} else {
						fprintf(stderr, "FIN received "
							"from client socket\n");
					}
					close(sockfd);
					FD_CLR(sockfd, &rset);
					clients[i] = -1;
				} else {
					if (write(sockfd, buf, n) == -1)
						err_exit("write error");
				}
				/* no more readable descriptor */
				if (--nready <= 0)
					break;  
			}
		}
	}
}

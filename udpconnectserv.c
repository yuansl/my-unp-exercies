#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void talk_with_client(int sockfd);
static void err_exit(const char *msg);
static void sigint_handler(int signo);

static int count;

int main(void)
{
	struct sockaddr_in addr;
	int sockfd;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err_exit("socket");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1024);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		err_exit("bind");
	
	talk_with_client(sockfd);

	close(sockfd);
	return 0;
}

static void talk_with_client(int sockfd)
{
	char buf[BUFSIZ];
	ssize_t n;
	struct sockaddr_in addr;
	socklen_t len;

	signal(SIGINT, sigint_handler);
	while (true) {
		len = sizeof(addr);
		if ((n = recvfrom(sockfd, buf, sizeof(buf), 0,
				  (struct sockaddr *) &addr, &len)) == -1)
			err_exit("recvfrom");
		count++;
	}
}

static void err_exit(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void sigint_handler(int signo)
{
	printf("\nReceived %d datagrams\n", count);
	exit(EXIT_SUCCESS);
}

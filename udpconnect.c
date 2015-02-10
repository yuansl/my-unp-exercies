#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void talk_with_server2(int sockfd);
static void talk_with_server(int sockfd);
static void err_exit(const char *msg);

int main(void)
{
	struct sockaddr_in addr;
	int sockfd;
	const char *ip = "49.140.49.227";

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err_exit("socket");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1024);
	inet_pton(AF_INET, ip, &addr.sin_addr);

	/* connected UDP socket make it peer-to-peer communication */
	if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		err_exit("connect");
	
	talk_with_server2(sockfd);
	
	close(sockfd);         
	return 0;
}

static void talk_with_server(int sockfd)
{
	char buf[BUFSIZ];
	ssize_t n;
	FILE *fp;

	if ((fp = fopen("server.c", "r")) == NULL) {
		perror("fopen");
		fp = stdin;
	} 

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if ((n = write(sockfd, buf, strlen(buf))) == -1)
			err_exit("write");
		
		memset(buf, 0, sizeof(buf));
		if ((n = read(sockfd, buf, sizeof(buf))) == -1)
			err_exit("read");
		buf[n] = '\0';
		fputs(buf, stdout);
	}
}

static void talk_with_server2(int sockfd)
{
	char buf[1400];
	int i;

	for (i = 0; i < 2000000; i++)
		if (write(sockfd, buf, 1400) == -1)
			err_exit("write");

}

static void err_exit(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

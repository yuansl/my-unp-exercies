#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define err_sys(errmsg) \
	do { perror(errmsg); exit(EXIT_FAILURE); } while (0)

static int tcp_connect(const char *host, const char *service);

int main(void)
{
	const char *ip = "172.17.0.2";
	const char *get_method = "GET / \n\n";
	char buf[BUFSIZ];
	int fd;
	ssize_t n;

	fd = tcp_connect(ip, "http");
	if (write(fd, get_method, strlen(get_method)) == -1)
		err_sys("write error");
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		write(STDOUT_FILENO, buf, n);

	return 0;
}

static int tcp_connect(const char *host, const char *service)
{
	int fd;
	struct addrinfo hints, *res, *pres;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	
	if (getaddrinfo(host, service, &hints, &res) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(h_errno));
	}
	for (pres = res; pres; pres = pres->ai_next) {
		fd = socket(pres->ai_family,
			    pres->ai_socktype, pres->ai_protocol);
		if (fd == -1)
			continue;
		if (connect(fd, pres->ai_addr, pres->ai_addrlen) == 0)
			break;
		close(fd);
	}
	if (pres == NULL)
		err_sys("connect error");
	freeaddrinfo(res);
	return fd;
}

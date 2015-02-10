#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

int main(void)
{
	char buf[BUFSIZ];
	int flag;
	int nready;
	fd_set rset, wset;

	flag = fcntl(0, F_GETFL, 0);
	fcntl(0, F_SETFL, flag | O_NONBLOCK);
	flag = fcntl(1, F_GETFL, 0);
	fcntl(1, F_SETFL, flag | O_NONBLOCK);

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	for (;;) {
		FD_SET(0, &rset);
		FD_SET(1, &wset);
		nready = select(2, &rset, &wset, NULL, NULL);
		if (nready < 0) {
			if (nready == 0)
				continue;
			break;
		}

		if (FD_ISSET(0, &rset)) {
			read(0, buf, sizeof(buf));
		}

		if (FD_ISSET(1, &wset)) {
			printf("what is the matter\n");
		}
	}
	printf("select error\n");
	return 0;
}

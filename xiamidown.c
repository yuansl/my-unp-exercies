#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_STRING     1024
#define MAX_QUERY      2048
#define PROTO_FTP      001
#define PROTO_HTTP     010
#define PROTO_DEFAULT  PROTO_FTP
#define err_sys(errmsg) \
	do { perror(errmsg); exit(EXIT_FAILURE); } while (0)

typedef struct {
	char host[MAX_STRING];
	char auth[MAX_STRING];
	char request[MAX_QUERY];
	char headers[MAX_QUERY];
	int proto;		/* FTP through HTTP proxies     */
	int proxy;
	long long int firstbyte;
	long long int lastbyte;
	int status;
	int fd;
	char *local_if;
} http_t;

typedef struct {
	int proto;
	int port;
	int proxy;
	char host[MAX_STRING];
	char dir[MAX_STRING];
	char file[MAX_STRING];
	char user[MAX_STRING];
	char pass[MAX_STRING];

	http_t http[1];
	long long int size;	/* File size, not 'connection size'..   */
	long long int currentbyte;
	long long int lastbyte;
	int fd;
	int enabled;
	int supported;
	int last_transfer;
	char *message;
	char *local_if;

	int state;
	pthread_t setup_thread[1];
} conn_t;

/* convert an integer to a string */
char *itostr(int n, char *str)
{
	char *p = str;
	int i, j, tmp;

	while (n > 0) {
		*p++ = n % 10 + '0';
		n /= 10;
	}
	*p = '\0';
	for (i = 0, j = strlen(str) - 1; i < j; i++, j--)
		tmp = str[i], str[i] = str[j], str[j] = tmp;
	return str;
}

int tcp_connect(char *hostname, int port, char *local_if)
{
	int fd;
	char service[16];    /* e.g http, ftp, domain... */
	struct addrinfo hints, *res, *pres;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	itostr(port, service);
	if (getaddrinfo(hostname, service, &hints, &res) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(h_errno));
		exit(EXIT_FAILURE);
	}
	for (pres = res; res; res = res->ai_next) {
		fd = socket(res->ai_family, res->ai_socktype,
			    res->ai_protocol);
		if (fd == -1)
			continue;
		if (connect(fd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close(fd);
	}
	if (res == NULL) {
		fprintf(stderr, "can't connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(pres);

	return fd;
}

void http_encode(char *s)
{
	char t[MAX_STRING];
	int i, j;

	for (i = j = 0; s[i]; i++, j++) {
		/* Fix buffer overflow */
		if (j >= MAX_STRING - 1) {
			break;
		}

		t[j] = s[i];
		if (s[i] == ' ') {
			/* Fix buffer overflow */
			if (j >= MAX_STRING - 3) {
				break;
			}

			strcpy(t + j, "%20");
			j += 2;
		}
	}
	t[j] = 0;

	strcpy(s, t);
}

static char string[MAX_STRING];
char *conn_url(conn_t *conn)
{
	if (conn->proto == PROTO_FTP)
		strcpy(string, "ftp://");
	else
		strcpy(string, "http://");

	if (*conn->user != 0 && strcmp(conn->user, "anonymous") != 0)
		sprintf(string + strlen(string), "%s:%s@",
			conn->user, conn->pass);

	sprintf(string + strlen(string), "%s:%i%s%s",
		conn->host, conn->port, conn->dir, conn->file);

	return (string);
}

int conn_set(conn_t *conn, const char *set_url)
{
	char url[MAX_STRING];
	char *i, *j;

	/* protocol://                                                  */
	if ((i = strstr(set_url, "://")) == NULL) {
		conn->proto = PROTO_DEFAULT;
		strncpy(url, set_url, MAX_STRING);
	} else {
		if (set_url[0] == 'f')
			conn->proto = PROTO_FTP;
		else if (set_url[0] == 'h')
			conn->proto = PROTO_HTTP;
		else {
			return (0);
		}
		strncpy(url, i + 3, MAX_STRING);
	}

	/* Split                                                        */
	if ((i = strchr(url, '/')) == NULL) {
		strcpy(conn->dir, "/");
	} else {
		*i = 0;
		snprintf(conn->dir, MAX_STRING, "/%s", i + 1);
		if (conn->proto == PROTO_HTTP)
			http_encode(conn->dir);
	}
	strncpy(conn->host, url, MAX_STRING);
	j = strchr(conn->dir, '?');
	if (j != NULL)
		*j = 0;
	i = strrchr(conn->dir, '/');
	*i = 0;
	if (j != NULL)
		*j = '?';
	if (i == NULL) {
		strncpy(conn->file, conn->dir, MAX_STRING);
		strcpy(conn->dir, "/");
	} else {
		strncpy(conn->file, i + 1, MAX_STRING);
		strcat(conn->dir, "/");
	}

	/* Check for username in host field                             */
	if (strrchr(conn->host, '@') != NULL) {
		strncpy(conn->user, conn->host, MAX_STRING);
		i = strrchr(conn->user, '@');
		*i = 0;
		strncpy(conn->host, i + 1, MAX_STRING);
		*conn->pass = 0;
	}
	/* If not: Fill in defaults                                     */
	else {
		if (conn->proto == PROTO_FTP) {
			/* Dash the password: Save traffic by trying
			   to avoid multi-line responses                */
			strcpy(conn->user, "anonymous");
			strcpy(conn->pass,
			       "mailto:axel-devel@lists.alioth.debian.org");
		} else {
			*conn->user = *conn->pass = 0;
		}
	}

	/* Password?                                                    */
	if ((i = strchr(conn->user, ':')) != NULL) {
		*i = 0;
		strncpy(conn->pass, i + 1, MAX_STRING);
	}
	/* Port number?                                                 */
	if ((i = strchr(conn->host, ':')) != NULL) {
		*i = 0;
		sscanf(i + 1, "%i", &conn->port);
	}
	/* Take default port numbers from /etc/services                 */
	else {

		if (conn->proto == PROTO_HTTP)
			conn->port = 80;
		else
			conn->port = 21;
	}

	return (conn->port > 0);
}
/* A hack for parse a douban music url
 * e.g: 
 * url: http://www.xiami.com/song/playlist/id/1626042130/type/1
 * and 
 * http://www.xiami.com/song/playlist/id/82479/object_name/default/object_id/0 
 * this is just a xml file, the real location (I mean *.mp3 or other form )is 
 * marked as <Location>, but the value string is encoded with an algorithm,
 * so, the follow parser will deal with it.
 */

/* Convert an URL to a conn_t structure	*/

char *parsestr(const char *str)
{
	int slen = strlen(str + 1);
	int rows = str[0] - '0';
	int cols = slen / rows;
	int right_rows = slen % rows;
	static char real_ret[MAX_STRING];	
	char new_str[rows][cols + 2];
	const char *p = str + 1;
	char ret[1024];
	int i, j, ct;
	
	for (i = 0; i < right_rows; i++) {
		strncpy(new_str[i], p, cols + 1);
		new_str[i][cols + 1] = '\0';
		p += cols + 1;
	}
	for (; i < rows; i++) {
		strncpy(new_str[i], p, cols);
		new_str[i][cols] = '\0';
		p += cols;
	}
	ct = 0;
	for (j = 0; j < cols; j++) 
		for (i = 0; i < rows; i++)
			ret[ct++] = new_str[i][j];

	for (i = 0; i < right_rows; i++)
		ret[ct++] = new_str[i][j];
	ret[ct] = '\0';
	p = ret;
	i = 0;
	while (*p) {
		if (*p == '%') {
			switch (*++p) {
			case '2':
				real_ret[i] = '/';
				break;
			case '3':
				if (*(p + 1) == 'F')
					real_ret[i] = '?';
				else if (*(p + 1) == 'D')
					real_ret[i] = '=';
				else
					real_ret[i] = ':';
				break;
			case '5':
				real_ret[i] = '0';
				break;
			default:
				fprintf(stderr, "Fatal error\n");
				exit(EXIT_FAILURE);
			}
			i++;
			p += 2;
			continue;
		} 
		real_ret[i++] = *p++;
	}
	return real_ret;
}

int http_connect(http_t *conn, int proto, char *proxy, char *host, int port,
		 char *user, char *pass)
{
	char base64_encode[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz0123456789+/";
	char auth[MAX_STRING];
	conn_t tconn[1];
	int i;

	strncpy(conn->host, host, MAX_STRING);
	conn->proto = proto;

	if (proxy != NULL) {
		if (*proxy != 0) {
			sprintf(conn->host, "%s:%i", host, port);
			if (!conn_set(tconn, proxy)) {
				/* We'll put the message in conn->headers, not in request */
				sprintf(conn->headers,
					"Invalid proxy string: %s\n", proxy);
				return (0);
			}
			host = tconn->host;
			port = tconn->port;
			conn->proxy = 1;
		} else {
			conn->proxy = 0;
		}
	}

	if ((conn->fd = tcp_connect(host, port, conn->local_if)) == -1) {
		/* We'll put the message in conn->headers, not in request */
		sprintf(conn->headers, "Unable to connect to server %s:%i\n",
			host, port);
		return (0);
	}

	if (*user == '\0') {
		*conn->auth = 0;
	} else {
		memset(auth, 0, MAX_STRING);
		snprintf(auth, MAX_STRING, "%s:%s", user, pass);
		for (i = 0; auth[i * 3]; i++) {
			conn->auth[i * 4] = base64_encode[(auth[i * 3] >> 2)];
			conn->auth[i * 4 + 1] =
			    base64_encode[((auth[i * 3] & 3) << 4) |
					  (auth[i * 3 + 1] >> 4)];
			conn->auth[i * 4 + 2] =
			    base64_encode[((auth[i * 3 + 1] & 15) << 2) |
					  (auth[i * 3 + 2] >> 6)];
			conn->auth[i * 4 + 3] =
			    base64_encode[auth[i * 3 + 2] & 63];
			if (auth[i * 3 + 2] == 0)
				conn->auth[i * 4 + 3] = '=';
			if (auth[i * 3 + 1] == 0)
				conn->auth[i * 4 + 2] = '=';
		}
	}

	return (1);
}

void http_disconnect(http_t *conn)
{
	if (conn->fd > 0)
		close(conn->fd);
	conn->fd = -1;
}

void http_addheader(http_t *conn, char *format, ...)
{
	char s[MAX_STRING];
	va_list params;

	va_start(params, format);
	vsnprintf(s, MAX_STRING - 3, format, params);
	strcat(s, "\r\n");
	va_end(params);

	strncat(conn->request, s, MAX_QUERY - strlen(conn->request) - 1);
}

void http_get(http_t *conn, char *lurl)
{
	*conn->request = 0;
	if (conn->proxy) {
		http_addheader(conn, "GET %s://%s%s HTTP/1.0",
			       conn->proto == PROTO_HTTP ? "http" : "ftp",
			       conn->host, lurl);
	} else {
		http_addheader(conn, "GET %s HTTP/1.1", lurl);
		http_addheader(conn, "Host: %s", conn->host);
		http_addheader(conn, "Connection: close");
	}
	if (*conn->auth)
		http_addheader(conn, "Authorization: Basic %s", conn->auth);
	if (conn->firstbyte) {
		if (conn->lastbyte)
			http_addheader(conn, "Range: bytes=%lld-%lld",
				       conn->firstbyte, conn->lastbyte);
		else
			http_addheader(conn, "Range: bytes=%lld-",
				       conn->firstbyte);
	}
}

int http_exec(http_t *conn)
{
	int nth, i;
	char s[2] = " ", *s2;

	http_addheader(conn, "");
	if (write(conn->fd, conn->request, strlen(conn->request)) == -1)
		err_sys("write error");

	*conn->headers = 0;
	/* Read the headers byte by byte to make sure we don't touch the
	   actual data                                                  */
	nth = 0;
	i = 0;
	while (1) {
		if (read(conn->fd, s, 1) <= 0) {
			/* We'll put the message in conn->headers, not in request */
			sprintf(conn->headers, "Connection gone.\n");
			return (0);
		}
		if (*s == '\r') {
			continue;
		} else if (*s == '\n') {
			if (nth == 0)
				break;
			nth = 0;
		} else {
			nth++;
		}
		conn->headers[i++] = *s;
	}
	conn->headers[i] = '\0';
	sscanf(conn->headers, "%*s %3i", &conn->status);
	s2 = strchr(conn->headers, '\n');
	*s2 = 0;
	strcpy(conn->request, conn->headers);
	*s2 = '\n';

	return (1);
}

char *http_header(http_t *conn, const char *header)
{
	char s[32];
	int i;

	for (i = 1; conn->headers[i]; i++)
		if (conn->headers[i - 1] == '\n') {
			sscanf(&conn->headers[i], "%31s", s);
			if (strcasecmp(s, header) == 0)
				return (&conn->headers[i + strlen(header)]);
		}

	return (NULL);
}

long long int http_size(http_t *conn)
{
	char *i;
	long long int j;

	if ((i = http_header(conn, "Content-Length:")) == NULL)
		return (-2);

	sscanf(i, "%lld", &j);
	return (j);
}

/* Decode%20a%20file%20name */
void http_decode(char *s)
{
	char t[MAX_STRING];
	int i, j, k;

	for (i = j = 0; s[i]; i++, j++) {
		t[j] = s[i];
		if (s[i] == '%')
			if (sscanf(s + i + 1, "%2x", &k)) {
				t[j] = k;
				i += 2;
			}
	}
	t[j] = 0;

	strcpy(s, t);
}

int main(int argc, char *argv[])
{
	char url[MAX_STRING], set_url[MAX_STRING], data[BUFSIZ], *p, *pos;
	conn_t conn[1];
	http_t http[1];
	ssize_t n;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s [URL]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	strcpy(url, argv[1]);

	if ((p = strstr(url, "/play?")) == NULL) {
		fprintf(stderr, "I am sorry, I can not do this\n");
		exit(EXIT_FAILURE);
	}

	*p = '\0';
	strcpy(set_url, url);
	p = strstr(p + 1, "/song");
	if (p != NULL)
		strcat(set_url, p);
	
	p = strchr(set_url, '#');
	if (p != NULL)
		*p = '\0';
	
	conn_set(conn, set_url);
	p = conn_url(conn);

	http_connect(http, conn->proto, NULL, conn->host, conn->port,
		     conn->user, conn->pass);
	http_get(http, p);
	http_exec(http);

	while ((n = read(http->fd, data, BUFSIZ)) > 0) {
		pos = data;
		while ((p = strstr(pos, "<location>")) != NULL) {
			sscanf(p + 10, "%1024[^<]s", http->request);
			pos = p;
			p = parsestr(http->request);
			strcpy(url, p);
			memset(conn, 0, sizeof(conn));
			conn_set(conn, url);
		
			p = strstr(url, ".mp3");
			
			if (p) {
				p = conn_url(conn);
				if (p == NULL) {
					fprintf(stderr, "Can't parse the url\n");
					break;
				}
				if (fork() == 0)
					execl("/usr/bin/axel", "axel", p, NULL);

			} else {
				fprintf(stderr, "parse failed\n");
				break;
			}
			pos++;
		}
	}
	
	if (n == -1)
		err_sys("read error");
	else if (n == 0)
		close(http->fd);
	
	while (waitpid(-1, NULL, WNOHANG) != -1)
		;

	return 0;
}

/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Xiong weilun, azardf4yy@gmail.com
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void proxy(int, int, struct sockaddr_in*);

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);

FILE* log_file;

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{

	/* Check arguments */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);

	}
	Signal(SIGPIPE, SIG_IGN); 
	log_file = Fopen("proxy.log", "a");


	int listenfd, connfd, port;
	socklen_t clientlen;
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char* haddrp;
	port = atoi(argv[1]);
	listenfd = Open_listenfd(port);
	while(1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		hp = Gethostbyaddr((const char*)&clientaddr.sin_addr.s_addr,
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		haddrp = inet_ntoa(clientaddr.sin_addr);
		printf("\n\n=========================================\n");
		printf("server connected to %s (%s)\n", hp->h_name, haddrp);


		proxy(connfd, port, &clientaddr);
		Close(connfd);
	}
	Fclose(log_file);
	exit(0);
}



/*
 * Real proxy function, pass data from server to client
 */

void proxy(int connfd, int port, struct sockaddr_in* sockaddr)
{
	size_t n;
	int log_size = 0;
	int parse;
	int ported;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char hostname[MAXLINE], pathname[MAXLINE];
	char log_entry[MAXLINE];

	rio_t rio;
	Rio_readinitb(&rio, connfd);
	struct sockaddr_in *addr = sockaddr;
	int hostfd =0;
	rio_t rio_host;	// real server rio
	while(1)
	{
		// Read request
		if((n = Rio_readlineb_w(&rio, buf, MAXLINE)) != 0)
		{
			sscanf(buf, "%s %s %s", method, uri, version);
			printf("Request: %s", buf);
			if (strcasecmp(method, "GET"))
			{
				printf("Unsupport method %s\n", method);
				printf("This proxy can only accept GET requests.\n\n\n");
				return;
			}
			// Parse the uri
			parse = parse_uri(uri, hostname, pathname, &ported);
			printf("uri: %s\nhostname: %s\npathname: %s\nport: %d\n", uri, hostname, pathname, ported);
			printf("parse flag: %d\n", parse);

			// Connect to host on hostfd
			if((hostfd = Open_clientfd(hostname, ported)) < 0)
			{
				printf("Open clientfd error\n");
				break;
			}

			// Send request to real host
			Rio_readinitb(&rio_host, hostfd);
			Rio_writen_w(hostfd, buf, strlen(buf));

			while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
			{
				Rio_writen_w(hostfd, buf, n);
				if (buf[0] == '\r')
					break;
				//printf("Client Request: %s", buf);	// detail request
				// html file hui luan ma
			}
			printf("Request Down\n");
			
			// Write log 
			format_log_entry(log_entry, addr, uri, log_size);
			fprintf(log_file, "%s %d\n", log_entry, log_size);
			fflush(log_file);
			printf("WriteLog down\n");

			// Read response from real host
			while((n = Rio_readnb_w(&rio_host, buf, MAXBUF) ) != 0)
			//while((n = Rio_readlineb_w(&rio_host, buf, MAXLINE) ) != 0)
			{
				Rio_writen_w(connfd, buf, n);
				//printf("Host Respon: %s\n", buf);
				log_size += n; // sizeof log
			}
			printf("Response Down\n");

			Close(hostfd);
			break;
		}
		else
		{
			printf("No request input.\n");
			break;
		}
	}
	printf("\n\n");
}




/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;

	if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
	}

	/* Extract the host name */
	hostbegin = uri + 7;
	hostend = strpbrk(hostbegin, " :/\r\n\0");
	len = hostend - hostbegin;
	strncpy(hostname, hostbegin, len);
	hostname[len] = '\0';

	/* Extract the port number */
	*port = 80; /* default */
	if (*hostend == ':')   
		*port = atoi(hostend + 1);

	/* Extract the path */
	pathbegin = strchr(hostbegin, '/');
	if (pathbegin == NULL) {
		pathname[0] = '\0';
	}
	else {
		pathbegin++;	
		strcpy(pathname, pathbegin);
	}

	return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		char *uri, int size)
{
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;

	/* Get a formatted time string */
	now = time(NULL);
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

	/* 
	 * Convert the IP address in network byte order to dotted decimal
	 * form. Note that we could have used inet_ntoa, but chose not to
	 * because inet_ntoa is a Class 3 thread unsafe function that
	 * returns a pointer to a static variable (Ch 13, CS:APP).
	 */
	host = ntohl(sockaddr->sin_addr.s_addr);
	a = host >> 24;
	b = (host >> 16) & 0xff;
	c = (host >> 8) & 0xff;
	d = host & 0xff;


	/* Return the formatted log entry string */
	sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}






/*
 * Robust I/O wrapper for proxy
 * no unix_error() so process won't exit
 */

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
	ssize_t rc;
	if((rc= rio_readnb(rp, usrbuf, n)) < 0)
	{
		printf("Warning: Rio_readnb_w error\n");
		return 0;
	}
	return rc;
}

ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes)
{
	ssize_t n;
	if ((n = rio_readn(fd, ptr, nbytes)) < 0)
	{
		printf("Warning: Rio_readn_w error\n");
		return 0;
	}
	return n;
}

void Rio_writen_w(int fd, void *usrbuf, size_t n)
{
	if(rio_writen(fd, usrbuf, n) != n)
		printf("Warning: Rio_writen_w error\n");
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
	ssize_t rc;
	if((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
	{
		printf("Warning: Rio_readlineb_w error\n");
		return 0;
	}
	return rc;
}







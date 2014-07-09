/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"
#define NTHREADS 8
#define SBUFSIZE 64
/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
int open_clientfd_ts(char *hostname, int port);
typedef struct{
     int clientfd;
     struct sockaddr_in clientaddr;
}fdaddr; 
typedef struct{
     fdaddr **buf;
     int n;
     int front;
     int rear;
     sem_t mutex;
     sem_t slots;
     sem_t items;
}sbuf_t;
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, fdaddr *client);
fdaddr *sbuf_remove(sbuf_t *sp);
void *doit(fdaddr *client);
void *thread(void *vargp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen); 
//ssize_t rio_writen_w(int fd, void *usrbuf, size_t n);
sem_t mutex1, mutex2;
sbuf_t sbuf;
/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    int listenfd, port, i;
    socklen_t clientlen;
    struct sockaddr_in clientaddrx;
    fdaddr *client;
    pthread_t tid;
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	exit(0);
    }
    signal(SIGPIPE, SIG_IGN);
    clientlen = sizeof(clientaddrx);
    Sem_init(&mutex1, 0, 1);
    Sem_init(&mutex2, 0, 1);
    sbuf_init(&sbuf, SBUFSIZE);
    port = atoi(argv[1]);
    listenfd = open_listenfd(port);
    for(i = 0; i < NTHREADS; i++)
          Pthread_create(&tid, NULL, thread, NULL);
    while (1) {
        client = (fdaddr *)Malloc(sizeof(fdaddr));
        client->clientfd = Accept(listenfd, (SA *)&(client->clientaddr), &clientlen);
        sbuf_insert(&sbuf, client);
    }
    exit(0);
}
void *thread(void *vargp)
{
      static int cnt = 1;
      Pthread_detach(pthread_self());
      printf("thread%d\n", cnt++);
      while(1){
        fdaddr *client = sbuf_remove(&sbuf);
        doit(client);
      }
}
/*doit*/
void *doit(fdaddr *client)
{
     char temp[MAXLINE], method[10], uri[MAXLINE], version[10], toServer[MAXLINE], logString[MAXLINE], toClient[MAXLINE];
     char serverName[MAXLINE], pathName[MAXLINE];
     int port, clientfd, serverfd, logfd, n, cnt = 0;
     rio_t clientRio, serverRio, logRio;
     struct sockaddr_in clientaddr;
     clientfd = client->clientfd;
     clientaddr = client->clientaddr;
     Free(client);
     printf("server client%d\n", clientfd);
     Rio_readinitb(&clientRio, clientfd);
     if ( Rio_readlineb_w(&clientRio, temp, MAXLINE)==0)
        return NULL;
     sscanf(temp, "%s %s %s", method, uri, version);
     if (strcasecmp(method, "GET")){
        clienterror(clientfd, method, "501", "Not Implement", "proxy does not implenment this method");
        return NULL;
     }
    parse_uri(uri, serverName, pathName, &port);
    //printf("server:%s\n", serverName);
    if ((serverfd = open_clientfd_ts(serverName, port)) < 0){//bug1
       strcpy(toClient, "can't connect server");
       printf("to client:%s\n", toClient);
       Rio_writen_w(clientfd, toClient, strlen(toClient));
       Close(clientfd);
       return NULL;
    }
    strcpy(toServer, method);
    strcat(toServer, " ");
    strcat(toServer, pathName);
    strcat(toServer, " ");
    strcat(toServer, version);
    strcat(toServer, "\r\n");
    while(strcmp(temp, "\r\n")){
        bzero(temp, MAXLINE); 
        Rio_readlineb_w(&clientRio, temp, MAXLINE);
        strcat(toServer, temp);
    }
    printf("start forwarding request from client%d to server%d...\n", clientfd, serverfd);
    printf("%s", toServer);
    Rio_writen_w(serverfd, toServer, sizeof(toServer));
    printf("client%d request over\n", clientfd);
    Rio_readinitb(&serverRio, serverfd);
    printf("start forwarding reply   from server%d to client%d\n", serverfd, clientfd);
    bzero(temp, MAXLINE);
    while ((n = Rio_readlineb_w(&serverRio, temp, MAXLINE)) > 0)
    {  
       printf("%s", temp);
       if ((rio_writen(clientfd, temp, n)) != n)
          break; 
       cnt += n;
       bzero(temp, MAXLINE);
    }
    printf("client%d replying over\n", clientfd);
    format_log_entry(logString, &clientaddr, uri, cnt);
    strcat(logString, "\r\n");
    printf("client%d write to log: %s\n", clientfd, logString); 
    P(&mutex2);
    logfd = Open("proxy.log", O_APPEND | O_WRONLY, S_IWUSR);
    Rio_readinitb(&logRio, logfd);
    Rio_writen(logfd, logString, strlen(logString));
    Close(logfd);
    V(&mutex2);
    printf("close client%d\n", clientfd);
    shutdown(serverfd, SHUT_RDWR);
    shutdown(clientfd, SHUT_RDWR);
    return NULL;
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
    short port;
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

    port = ntohs(sockaddr->sin_port);
    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d : %u %s %d", time_str, a, b, c, d, port, uri, size);
}
/*open_clientfd_ts, thread safe*/
int open_clientfd_ts(char *hostname, int port)
{    
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;
    if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    bzero((char * )&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    P(&mutex1);
    if((hp = gethostbyname(hostname)) == NULL)
        return -2;
    bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    V(&mutex1);
    serveraddr.sin_port = htons(port);
    if(connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return clientfd;
}     


/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes) 
{
    ssize_t n;
  
    if ((n = rio_readn(fd, ptr, nbytes)) < 0){
	printf("Rio_readn error");
        return 0 ;
    }
    return n;
}

void Rio_writen_w(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n)
  
       printf("Rio_writen_w error");
    
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;
    rc = rio_readlineb(rp, usrbuf, maxlen);
    return rc;
}
/* $begin rio_writen */
/* $end n rio_writen */
void sbuf_init(sbuf_t *sp, int n)
{
     sp->buf = Calloc(n,sizeof(fdaddr *));
     sp->n = n;
     sp->front = sp->rear = 0;
     Sem_init(&sp->mutex, 0, 1);
     Sem_init(&sp->slots, 0, n);
     Sem_init(&sp->items, 0, 0);
}
void sbuf_deinit(sbuf_t *sp)
{
     Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, fdaddr *item)
{
     P(&sp->slots);
     P(&sp->mutex);
     sp->buf[(++sp->rear)%(sp->n)] = item;
     V(&sp->mutex);
     V(&sp->items);
}
fdaddr *sbuf_remove(sbuf_t *sp)
{
     fdaddr *item;
     P(&sp->items);
     P(&sp->mutex);
     item = sp->buf[(++sp->front)%(sp->n)];
     V(&sp->mutex);
     V(&sp->slots);
     return item;
}


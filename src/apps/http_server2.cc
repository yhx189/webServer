#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc,i;
  fd_set readlist;
  fd_set connections;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }
  if(toupper(*(argv[1])) == 'K'){
  	minet_init(MINET_KERNEL);
  }else if (toupper(*argv[1]) == 'U'){
  	minet_init(MINET_USER);
  }else{
  	fprintf(stderr, "First argument should be k or u\n");
	exit(-1);
  }
  sock = 0;
  /* initialize and make socket */
  if( (sock = minet_socket(SOCK_STREAM)) < 0){
  	fprintf(stderr, "cannot create socket\n");
	exit(-1);
  }
  /* set server address*/
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(server_port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  /* bind listening socket */

  if(minet_bind(sock, (sockaddr_in*)&sa) < 0){
  	fprintf(stderr, "cannot bind socket\n");
	minet_close(sock);
	exit(-1);
  }
  /* start listening */

  if(minet_listen(sock, 10) < 0){
  	fprintf(stderr, "cannot start listening\n");
	minet_close(sock);
	exit(-1);
  }
  FD_ZERO(&connections);
  FD_SET(sock, &connections);
  maxfd = sock;
  /* connection handling loop */
  while(1)
  {
    /* create read list */
	FD_ZERO(&readlist);
	for(i = 0; i < maxfd; i++){
		if(FD_ISSET(i, &connections))
			FD_SET(i, &readlist);
	}

	
    /* do a select */
	if( minet_select(maxfd+1, &readlist, 0, 0, 0) < 1){
		fprintf(stderr, "cannot select\n");
		minet_close(sock);
		exit(-1);
		
	}else{
		fprintf(stdout, "select finished\n");
	}

     int j;
    /* process sockets that are ready */
     for(i = 0; i <= maxfd; i++){
      /* for the accept socket, add accepted connection to connections */
        if (i == sock){
		memset(&sa2, 0, sizeof(sa2));
		if((sock2 = minet_accept(i, &sa2)) < 0){
	
			fprintf(stderr, "cannot accept socket\n");
			minet_close(i);
			exit(-1);
		}
		FD_SET(sock2, &connections);
		if(sock2 > maxfd)
			maxfd = sock2;
        }
        else /* for a connection socket, handle the connection */{
	   	rc = handle_connection(i);
	   	if(rc < 0){
	   		fprintf(stderr, "cannot handle connection\n");
	   		FD_CLR(i, &connections);
			if(i == maxfd){
				for(j = maxfd - 1; j >= 0; j--){
				if(FD_ISSET(j, &connections))
					maxfd = j;
				} 
			}
	   	}
      	}
      }
  }
}

int handle_connection(int sock2)
{
  //char filename[FILENAMESIZE+1];
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/

  if(minet_read(sock2, buf, BUFSIZE) < 0){
	  fprintf(stderr, "cannot read on sock2\n");
	  minet_close(sock2);
	  exit(-1);
  }
  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
    /* try opening the file */
  char * get = strtok(buf, " \r\n");
  char * fname = strtok(NULL, " \r\n");
  char * version = strtok(NULL, " \r\n");

  char path[FILENAMESIZE+1];
  memset(path, 0, FILENAMESIZE +1);
  getcwd(path, FILENAMESIZE);
  strncpy(path + strlen(path), "/", 1);
  strncpy(path + strlen(path), fname, strlen(fname));

  char*data;
  if(stat(path, &filestat) < 0){
  	ok = false;
  }else{
  	datalen = filestat.st_size;
	FILE * fp = fopen(path, "r");
	data = (char*) malloc(datalen);
	memset(data, 0, datalen);
	fread(data, 1, datalen, fp);

  
  }
  /* send response */
  if (ok)
  {
    /* send headers */
	sprintf(ok_response, ok_response_f, datalen);
	if(writenbytes(sock2, ok_response, strlen(ok_response)) < 0){
		fprintf(stderr, "cannot send file header\n");
		minet_close(sock2);
		exit(-1);
	}
	/* send file */
	if(writenbytes(sock2, data, datalen) < 0){
		fprintf(stderr, "cannot send file data\n");
		minet_close(sock2);
		exit(-1);
	}
	minet_close(sock2);
	return 0;
    
  }
  else	// send error response
  {
	  writenbytes(sock2, notok_response, strlen(notok_response));
	  minet_close(sock2);
	  return -1;
  }

  /* close socket and free space */

  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}


#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
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
  int rc;

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
  }else if (toupper(*(argv[1])) == 'U'){
  	minet_init(MINET_USER);
  }else{
  	fprintf(stderr, "First argument should be k or u\n");
	exit(-1);
  }
  sock = 0;
  /* initialize and make socket */
  if ( (sock = minet_socket(SOCK_STREAM)) < 0){
  	fprintf(stderr, "cannot create socket\n");
	exit(-1);
  
  }
  /* set server address*/
  memset(&sa, 0, sizeof(sa));

  sa.sin_family = AF_INET;
  sa.sin_port = htons(server_port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  /* bind listening socket */ 

  if (minet_bind(sock,(sockaddr_in*) &sa) < 0){
  	fprintf(stderr, "cannot bind socket\n");
  	minet_close(sock);
	exit(-1);
  }else{
  	fprintf(stdout, "socket bound!\n");
  }
  /* start listening */
  if(minet_listen (sock, 10) < 0){
  	fprintf(stderr, "cannot start listening\n");
  	minet_close(sock);
	exit(-1);
  }else{
  	fprintf(stdout, "start listening!\n");
  }
  /* connection handling loop */
  while(1)
  {
    /* handle connections */
    	memset(&sa2, 0, sizeof(sa2));
	if( (sock2 = minet_accept(sock, &sa2)) < 0){
		fprintf(stderr, "cannot accpet socket\n");
  		minet_close(sock);	
		exit(-1);
	}	  
	fprintf(stdout, "socket accepted, start conncection...\n");
       rc = handle_connection(sock2);
       if (rc < 0){
       		fprintf(stderr, "cannot handle connection\n");
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
                         "<h2>404 FILE NOT FOUND</h2>\n"
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/
  if (minet_read(sock2, buf, BUFSIZE) < 0){
  	fprintf(stderr, "cannot read on sock2\n");
  	minet_close(sock2);
	exit(-1);
  }else{
  
  	fprintf(stdout, "read from input")
  }
  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/


    /* try opening the file */
  char *get = strtok(buf, "\r\n");
  char *filename = strtok(NULL, "\r\n");
  char *version = strtok(NULL, "\r\n");

  char path[FILENAMESIZE + 1];
  memset(path, 0, FILENAMESIZE + 1);
  getcwd(path, FILENAMESIZE);
  strncpy(path + strlen(path), filename, strlen(filename));

  char *data;
  if(stat(path, &filestat) < 0){
  	fprintf(stderr, "cannot get file\n");
  	minet_close(sock2);
	exit(-1);
  
  }else{
  	datalen = filestat.st_size;
	FILE * fp = fopen(path, "r");
	data = (char*)malloc(datalen);
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
    if(writenbytes(sock2, data, datalen) < 0){
        fprintf(stderr, "cannot send file data\n");
  	minet_close(sock2);
	exit(-1);

    }
    minet_close(sock2);
    return 0;
    /* send file */
  }
  else // send error response
  {
	  writenbytes(sock2, notok_response, strlen(notok_response));
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


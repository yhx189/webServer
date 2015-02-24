#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>


#define FILENAMESIZE 100
#define BUFSIZE 1024

typedef enum \
{NEW,READING_HEADERS,WRITING_RESPONSE,READING_FILE,WRITING_FILE,CLOSED} states;

typedef struct connection_s connection;
typedef struct connection_list_s connection_list;

struct connection_s
{
  int sock;
  int fd;
  char filename[FILENAMESIZE+1];
  char buf[BUFSIZE+1];
  char *endheaders;
  bool ok;
  long filelen;
  states state;
  int headers_read,response_written,file_read,file_written;

  connection *next;
};

struct connection_list_s
{
  connection *first,*last;
};

void add_connection(int,connection_list *);
void insert_connection(int,connection_list *);
void init_connection(connection *con);


int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
void read_headers(connection *);
void write_response(connection *);
void read_file(connection *);
void write_file(connection *);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc;
  fd_set readlist,writelist;
  connection_list connections;
  connection *i;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server3 k|u port\n");
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
  }else if(toupper(*(argv[1])) == 'U'){
  	minet_init(MINET_USER);
  }else{
  	fprintf(stderr, "First argument should be k or u\n");
	exit(-1);
  }

  /* initialize and make socket */
  sock = 0;
  if((sock = minet_socket(SOCK_STREAM)) < 0){
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
  }else{
  //	fprintf(stdout, "start listening...\n");
  }

  //memset(&connections, 0, sizeof(connections));
  //i = (connection *)malloc(sizeof(connection));
  //connections.first = (connection*)malloc(sizeof(connection));
  //init_connection(connections.first);
  //connections.first->state = NEW;
  //connections.first->sock = sock;
  //connections.first->next = NULL;
  connections.first = NULL;
  connections.last = NULL;
  maxfd = sock;
  /* connection handling loop */
  while(1)
  {
    /* create read and write lists */
	FD_ZERO(&readlist);
	FD_ZERO(&writelist);
	FD_SET(sock, &readlist);
	for (i = connections.first; i!=NULL; i = i->next){
		if(i->state == CLOSED){
			continue;
		}else if ((i->state == NEW)||(i->state == READING_HEADERS)){
		
			FD_SET(i->sock, &readlist);
			if(i->sock > maxfd)
				maxfd = i->sock;
		}else if(i->state == READING_FILE){
			FD_SET(i->fd, &readlist);
		}else if((i->state == WRITING_RESPONSE) ||(i->state == WRITING_FILE)){
			FD_SET(i->sock, &writelist);
			if(i->sock > maxfd)
				maxfd = i->sock;
		
		}
	}
//		if( (i->sock <= maxfd) ){
//			FD_SET(i->sock, &readlist);
//			fprintf(stdout, "sock %d added to read list\n", i->sock);
//		}
//		if( (i->state == READING_FILE) ){
//			FD_SET(i->fd, &readlist);
//			fprintf(stdout, "fd %d added to read list\n", i->fd);
//		}
//	}

//	FD_ZERO(&writelist);
//	fprintf(stdout, "write list initiated, maxfd = %d\n", maxfd);
//	for (i = connections.first; i!= NULL; i = i->next){
//		fprintf(stdout, "i->sock = %d\n", i->sock);
//		if( (i->sock <= maxfd) ){
//			FD_SET(i->sock, &writelist);
//			fprintf(stdout, "sock %d added to write list\n", i->sock);
//		}
//	}

    /* do a select */
    /* process sockets that are ready */

//	if(minet_select(maxfd+1, &readlist, &writelist, 0, 0) < 1){

	rc = minet_select(maxfd+1, &readlist, &writelist, 0, 0);
	while((rc < 0) && (errno == EINTR))
		rc = minet_select(maxfd+1,&readlist, &writelist, 0, 0);
	if(rc < 0){
		fprintf(stderr, "cannot select\n");
		minet_close(sock);
		exit(-1);
	}
//	fprintf(stdout, "read select finished\n");
	for(i=connections.first; i!=NULL; i = i->next){
		if(i->state == CLOSED)
			continue;
		if(i->state == NEW){
			init_connection(i);
			i->state = READING_HEADERS;
			read_headers(i);
		} else if((i->state == READING_HEADERS) &&(FD_ISSET(i->sock, &readlist)))
		{
			read_headers(i);
		}else if(i->state == WRITING_RESPONSE && FD_ISSET(i->sock,&writelist)){
		write_response(i);
		}else if(i->state == READING_FILE && FD_ISSET(i->fd,&readlist)){
		
			read_file(i);
		}else if(i->state == WRITING_FILE && FD_ISSET(i->sock,&writelist)){
			write_file(i);
		}



	}
	if(FD_ISSET(sock, &readlist)){
		sock2=minet_accept(sock,&sa2);
		if(sock2<0){
			fprintf(stderr, "cannot accept socket\n");
			minet_close(sock2);
			exit(-1);
		}
		insert_connection(sock2, &connections);
		fcntl(sock2, F_SETFL, O_NONBLOCK);
	}
	}

/****************
		int j;
		for(j = 0; j <= maxfd; j++){
			if(j == sock && FD_ISSET(j, &readlist) ){
			
				memset(&sa2, 0, sizeof(sa2));
				if((sock2 = minet_accept(j, &sa2)) <0){
					fprintf(stderr, "cannot accept socket\n");
					minet_close(j);
					exit(-1);
				}
				fprintf(stdout, "socket accepted!\n");
				fcntl(sock2, F_SETFL, O_NONBLOCK);
				insert_connection(sock2, &connections);
		//		fprintf(stdout, "sock2 = %d\n",sock2);
				if(sock2 > maxfd)
					maxfd = sock2;
			}
			else if (FD_ISSET(j, &readlist)){
				for(i=connections.first; i!= NULL; i=i->next){
					if(i->sock == j){
						i->state = READING_HEADERS;
						fprintf(stdout, "start reading headers...\n");
						read_headers(i);
						break;
					}
					
				}	
				for(i=connections.first; i!= NULL; i=i->next){
					if(i->fd == j){
						i->state = READING_FILE;
						fprintf(stdout, "start reading file...\n");
						read_file(i);
						break;
					}
					
				}

			}
		
//		}

//	}

//	if(minet_select(maxfd+1, 0, &writelist, 0, 0) < 1){
//		fprintf(stderr, "cannot select writelist\n");
//		minet_close(sock);
//		exit(-1);
//	}else{
//	fprintf(stdout, "writelist select finished, start writing file...\n");

//		int j;
//		for(j = 0; j <= maxfd; j++){
			if(FD_ISSET(j, &writelist)){
				for(i = connections.first; i != NULL; i = i->next){
					if(i->sock == j)
						break;
				}
			
			if(i->state == CLOSED)
				FD_CLR(i->fd, &readlist);
			write_file(i);
			//FD_CLR(j, & writelist);
			if(i->file_written == i->filelen){
			// if you've written the whole file
			// remove i from connections
				connection * ret = (connection*)malloc(sizeof(connection));
				for(ret=connections.first;ret!=NULL;ret=ret->next){
					if(ret->next == i)
						break;
				}
				ret->next = i->next;
				FD_CLR(j, &writelist);
				if(j == maxfd){
					int k;
					for (k=j-1;k>=0;k--){
						if(FD_ISSET(k, &writelist))
						{
							maxfd = k;
							break;
						}
					}
				
				}
			}
			}
		}
	}	

    }
*****************/
}

void read_headers(connection *con)
{
  /* first read loop -- get request and headers*/
  int sock2 = con->sock;
  char buf[BUFSIZE+1];
  struct stat filestat;
  

  if(minet_read(sock2, buf, BUFSIZE) < 0){
  	fprintf(stderr, "cannot read on sock2\n");
	minet_close(sock2);
	exit(-1);
  }
  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
  char * get = strtok(buf, " \r\n");
  char * fname = strtok(NULL, " \r\n");
  char * version = strtok(NULL, " \r\n");

  memset(con->filename, 0, FILENAMESIZE + 1);
  getcwd(con->filename, FILENAMESIZE);
  strncpy(con->filename + strlen(con->filename), "/", 1);
  strncpy(con->filename + strlen(con->filename), fname, strlen(fname));

  /* get file name and size, set to non-blocking */
  char * data;
  int fp;
  
  if(stat(con->filename, &filestat) < 0){
  	con->ok = false;
  }else{
  	con->ok = true;
	con->filelen = filestat.st_size;
	fp = open(con->filename, O_RDONLY);
	con->fd = fp;
	fcntl(fp, F_SETFL, O_NONBLOCK );
	con->state = WRITING_RESPONSE;
	write_response(con);
  }
    /* get name */
    
    /* try opening the file */
      
	/* set to non-blocking, get size */
  
	//write_response(con);
}

void write_response(connection *con)
{
  int sock2 = con->sock;
  int rc;
  int written = con->response_written;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  /* send response */
  if (con->ok)
  {
    /* send headers */
	  sprintf(ok_response, ok_response_f, con->filelen);
	  rc = writenbytes(sock2, ok_response + written, strlen(ok_response) - written);
          if(rc < 0)
		  fprintf(stderr, "cannot write response\n");

	  con->state = WRITING_RESPONSE;
//	  fprintf(stdout, "writing response on sock %d\n", sock2);
	  con->state = READING_FILE;
	  read_file(con);
  }
  else
  {
	  writenbytes(sock2, notok_response, strlen(notok_response));
	  minet_close(sock2);
	  return;
  }  
}

void read_file(connection *con)
{
  int rc;

    /* send file */
  con->state = READING_FILE;
  //  rc = readnbytes(con->fd,con->buf,BUFSIZE);

  rc = read(con->fd,con->buf,BUFSIZE);
  if (rc < 0)
  { 
    if (errno == EAGAIN)
      return;
    fprintf(stderr,"error reading requested file %s\n",con->filename);
    return;
  }
  else if (rc == 0)
  {
    con->state = CLOSED;
    close(con->fd);
    minet_close(con->sock);
  }
  else
  {
    con->file_read = rc;
    con->state = WRITING_FILE;
    write_file(con);
  }
}

void write_file(connection *con)
{
  int towrite = con->file_read;
  int written = con->file_written;
  int rc = writenbytes(con->sock,  con->buf+written, towrite-written);
//  fprintf(stdout, "the buf is %s\n", con->buf+written);
  if (rc < 0)
  {
    if (errno == EAGAIN)
      return;
    minet_perror("error writing response ");
    con->state = CLOSED;
    minet_close(con->sock);
    return;
  }
  else
  {
    con->file_written += rc;
    if (con->file_written == towrite)
    {
      con->state = READING_FILE;
      con->file_written = 0;
      read_file(con);
    }
    else
	    printf("shouldn't happen\n");
  }
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


// inserts a connection in place of a closed connection
// if there are no closed connections, appends the connection 
// to the end of the list

void insert_connection(int sock,connection_list *con_list)
{
  connection *i;
  for (i = con_list->first; i != NULL; i = i->next)
  {
    if (i->state == CLOSED)
    {
      i->sock = sock;
      i->state = NEW;
      return;
    }
  }
  add_connection(sock,con_list);
}
 
void add_connection(int sock,connection_list *con_list)
{
  connection *con = (connection *) malloc(sizeof(connection));
  con->next = NULL;
  con->state = NEW;
  con->sock = sock;
  if (con_list->first == NULL)
    con_list->first = con;
  
  if (con_list->last != NULL)
  {
    con_list->last->next = con;
    con_list->last = con;
  //  fprintf(stdout, "sock %d added to connections\n", sock);
  }
  else
  {	
//	  fprintf(stdout, "first sock %d added to connections\n", sock);
//          con_list->first->next = con;	  
	  con_list->last = con;
  }
}

void init_connection(connection *con)
{
  con->headers_read = 0;
  con->response_written = 0;
  con->file_read = 0;
  con->file_written = 0;
}

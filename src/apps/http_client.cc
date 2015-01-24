#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    char * bptr2 = NULL;
    char * endheaders = NULL;
   
    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
    sock = minet_socket(SOCK_STREAM);
 //   if ((sock = minet_socket( SOCK_STREAM)) < 0) {
//	      minet_perror("socket");
//	      exit(-1); 
  //  }
    
    // Do DNS lookup
    /* Hint: use gethostbyname() */

    /* set address */
    site = gethostbyname(server_name);
    if (site == NULL){
            fprintf(stderr, "fail to get site\n");
	    exit(-1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    memcpy((char*)&(sa.sin_addr),(char*)site->h_addr, site->h_length);
    sa.sin_port = htons(server_port);
    /* connect socket */
    
    if (minet_connect (sock, &sa) != 0){
    	minet_perror("cannot connect to minet\n");
	minet_close(sock);
	exit(-1);
    }
    else{
    	fprintf(stdout, "1 debugging\n");
    }
    	   
    /* send request */

    req = (char*) malloc(15 + strlen(server_path));
    sprintf(req, "GET %s HTTP/1.0\n\n", server_path);
    if (write_n_bytes(sock, req, strlen(req)) < 0 ) {
    	minet_perror("cannot send\n");
	minet_close(sock);
	exit(-1);
    }

    else{
    	fprintf(stdout, "2 debugging\n");
    }


    FD_ZERO(&set);
    FD_SET(sock, &set);
    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
   // if (FD_ISSET(sock, &set) == 0){
    
    //	  minet_perror("could not add sock to set!");
    //      minet_close(sock);
//	  exit(-1);
  //  }
    fprintf(stdout, "3 debugging\n");
 if (minet_select(sock+1, &set, 0, 0, 0) < 1){
    
   	   fprintf(stderr, "select!\n");
	   minet_close(sock);
           exit(-1);
    }
 fprintf(stdout, "debugging\n");

    /* first read loop -- read headers */
    if (minet_read(sock, buf, BUFSIZE) < 0){
       minet_perror("recv");
       exit(-1);
    }

    fprintf(stdout, "debugging\n");

    char * head = buf;
    while(head[-1] != ' ')
	    head++;
    char tmp[4];
    strncpy(tmp, head, 3);
    tmp[4] = '\0';
    rc = atoi(tmp);

    fprintf(stdout, "rc = %d\n", rc);
    sscanf(buf, "%*s %d", &rc);
    ok = (rc == 200);
    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200

    /* print first part of response */

    /* second read loop -- print out the rest of the response */
    fprintf(wheretoprint, "The status was %d.\n", rc);
    char *rsp = buf;

    while (!(rsp[0] == '\n' && rsp[-2] == '\n'))
		    	    rsp++;

	        free(req);
    /*close socket and deinitialize */


    if (ok) {
	 fprintf(wheretoprint, "%s", rsp);
  	 while ((datalen = minet_read(sock, buf, BUFSIZE)) > 0) {
		buf[datalen] = '\0';
		 fprintf(wheretoprint, "%s", buf);
	 }
	      
	  minet_close(sock);
          minet_deinit();
	  return 0;
    
    } else {
	    minet_close(sock);
	    minet_deinit();
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}



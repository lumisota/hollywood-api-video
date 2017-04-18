/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 *
 * Saba Ahsan, November 2016
 * Reusing code under GPLv2 to integrate TCP Hollywood. 
 *
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "media_sender.h"
#include "../common/http_ops.h"

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

uint8_t     Hollywood = 0;
uint32_t offset = 0;     /*offset added to last 4 bytes of the message*/
uint32_t stream_seq = 0;

void * accept_request(void * a);
void bad_request(int);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int serve_file(void * sock, const char *filename, int seq);
int startup(u_short *);
int check_arguments(int argc, char* argv[], u_short * port);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void * accept_request(void * a)
{
    void * sock;
    hlywd_sock h_sock;
    int num_of_requests = 0;
    char buf[HTTPHEADERLEN];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int seq         = 0 ;
    
    if(Hollywood)
    {

        int client = *((int *)a);
        if (hollywood_socket(client, &h_sock, 0, 0) != 0) {
            printf("Unable to create Hollywood socket\n");
            return NULL;
        }
        printf("Hollywood socket initialized for %d\n", client); fflush(stdout);
        sock = &h_sock;
        
    }
    else
    {
        sock = a ;
    }
    
    while (num_of_requests < 100)
    {
        char *query_string = NULL;

        numchars = get_html_headers(sock, buf, HTTPHEADERLEN, Hollywood);
        
        if (numchars == 0 )
            break;
        num_of_requests++;
        i = 0; j = 0;
        while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
        {
            method[i] = buf[j];
            i++; j++;
        }
        method[i] = '\0';

        if (strcasecmp(method, "GET"))
        {
            unimplemented(sock, Hollywood);
            return NULL;
        }

       // printf("received a GET request: \n %s \n", buf);
        i = 0;
        while (ISspace(buf[j]) && (j < sizeof(buf)))
            j++;
        while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
        {
            url[i] = buf[j];
            i++; j++;
        }
        url[i] = '\0';

        if (strcasecmp(method, "GET") == 0)
        {
            query_string = url;
            while ((*query_string != '?') && (*query_string != '\0'))
                query_string++;
        }

        sprintf(path, "testfiles%s", url);
        if (path[strlen(path) - 1] == '/')
            strcat(path, "index.html");
        //printf("Requested path: %s\n", path); fflush(stdout);
        if (stat(path, &st) == -1)
        {
            not_found(sock, Hollywood);
        }
        else
        {
            int ret;
            if ((st.st_mode & S_IFMT) == S_IFDIR)
                strcat(path, "/index.html");
            ret = serve_file(sock, path, seq);
            if (ret > 0)
                seq = ret;
            
        }
    
    }
    printf("\nClosing client connection %d after %d Requests \n", *(int *)a, num_of_requests);
    close(*(int *)a);
    return NULL;
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "<P>Your browser sent a bad request, ");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "such as a POST without a Content-Length.\r\n");
 send(client, buf, sizeof(buf), 0);
}



/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
 perror(sc);
 exit(1);
}



/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
int serve_file(void * sock, const char *filename, int seq)
{
    FILE *resource = NULL;
    char buf[1024];
    int ret;

    buf[0] = 'A'; buf[1] = '\0';

    resource = fopen(filename, "r");
    if (resource == NULL)
        ret = not_found(sock, Hollywood);
    else
    {
        printf("Sending file : %s using %d\r", filename, Hollywood); fflush(stdout); 

        if ( send_resp_headers(sock, filename, Hollywood) < 0)
        {
            printf("Failure sending response headers to client \n");
            return -1;
        }
        
        if((strstr(filename,".mp4")!=NULL || strstr(filename,".ts")!=NULL || strstr(filename,".m4s")!=NULL || strstr(filename,".six")!=NULL) && Hollywood==1)
        {
            ret = send_media_over_hollywood((hlywd_sock *)sock, resource, seq);
        }
        else
        {
            if(Hollywood)
            {
                /*reset offset and seq numbers whenever a non-video (mpd) file is requested*/
                offset = 0;     /*offset added to last 4 bytes of the message*/
                stream_seq = 0;

            }
            ret = cat(sock, resource, Hollywood);
        }
        fclose(resource);
    }
   // printf("File closed\n");
    return ret;
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}


/**********************************************************************/

int check_arguments(int argc, char* argv[], u_short * port)
{
    int i; 
    for(i=1; i<argc; i++)
    {
        if(strcmp(argv[i], "--port")==0)
        {
            ++i;
            if(i<argc)
                *port=atoi(argv[i]);
            else
            {
                printf ("Invalid arguments\n");
                printf("Usage with TCP Hollywood : %s --port <port number> --hollywood\n", argv[0]);
                printf("Usage with TCP only : %s --port <port number> \n", argv[0]);
                printf("Usage with TCP with auto port : %s \n", argv[0]);
                return -1;
            }
        }
        else if(strcmp(argv[i], "--hollywood")==0)
            Hollywood=1;
        else
        {
            printf ("Invalid arguments\n");
            printf("Usage with TCP Hollywood : %s --port <port number> --hollywood\n", argv[0]);
            printf("Usage with TCP only : %s --port <port number> \n", argv[0]);
            printf("Usage with TCP with auto port : %s \n", argv[0]);
            return -1;

        }
    }
    
    return 0;
}

/**********************************************************************/


int main(int argc, char *argv[])
{
    int server_sock = -1;
    u_short port = 0;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;
    
    /* Check for hostname parameter */
    if (argc > 1) {
        if((check_arguments(argc, argv, &port))<0)
            return(0);
    }

    server_sock = startup(&port);
    printf("httpd running on port %ud\n", port);

    while (1)
    {
        client_sock = accept(server_sock,
                       (struct sockaddr *)&client_name,
                       &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        printf("\nAccepting request for socket %d (from %d)\n", client_sock, server_sock); fflush(stdout);
        /* accept_request(client_sock); */
        if (pthread_create(&newthread , NULL, accept_request, &client_sock) != 0)
            perror("pthread_create");
    }

    close(server_sock);

    return(0);
}

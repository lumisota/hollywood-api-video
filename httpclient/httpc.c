#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include "mm_parser.h"




#define ISspace(x) isspace((int)(x))
/**********************************************************************/

int receive_video_over_hlywd (int fd, struct metrics * metric)
{
    printf("Receiving file over TCP Hollywood\n");
    metric->Hollywood = 1;
    /* Create a Hollywood socket */
    /*TODO: What is the third argument?, need to make it user assigned*/
    if (hollywood_socket(fd, &(metric->h_sock), 0) != 0) {
        printf("Unable to create Hollywood socket\n");
        return -1;
    }
    
    return (mm_parser(metric));
}

/**********************************************************************/

int receive_video_over_tcp (int fd, struct metrics * metric)
{
    printf("Receiving file over TCP\n");
    metric->Hollywood=0;
    return (mm_parser(metric));
}

/**********************************************************************/
int get_html_headers(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    
    n = recv(sock, buf, 4, 0);
    i=4;
    
    while ((i < size - 1) && strncmp(buf+i-4,"\r\n\r\n",4)!=0)
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            buf[i] = c;
            i++;
        }
        else
            break;
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/

int receive_response(int fd, struct metrics * metric)
{
    char buf[1024];
    char response[25];
    char unneeded[128];
    int numchars;
    int i, j;
    
    get_html_headers(fd, buf, 1024);

    if(strstr(buf, "200 OK")==NULL)
    {
        printf("Request Failed: \n");
        printf("%s",buf);
        return -1;
    }
        
    if(strstr(buf, "video/mp4")!=NULL)
    {
        return (receive_video_over_tcp(fd, metric));
    }
    else if(strstr(buf, "video/hlywd")!=NULL)
    {
        return receive_video_over_hlywd(fd, metric);
    }
    else
        printf("No markers found"); 
    
    return -1; 
}


/**********************************************************************/

int send_get_request(int fd, char * host, char * filename)
{
    char request[512]= "";
    sprintf(request, "GET /%s HTTP/1.1\r\n\r\n", filename);
    if(write(fd, request, strlen(request))<0)
        return -1;
    return 0;
}


/**********************************************************************/

int check_arguments(int argc, char* argv[], char * port, char * host, char * file)
{
    for(int i=1; i<argc; i++)
    {
        if(strcmp(argv[i], "--port")==0)
        {
            ++i;
            if(i<argc)
                strcpy(port, argv[i]);
            else
            {
                printf ("Invalid arguments\n");
                printf("Usage : %s --port <port number> --host <hostname> --file <filename>\n", argv[0]);
                return -1;
            }
        }
        else if(strcmp(argv[i], "--host")==0)
        {
            ++i;
            if(i<argc)
                strcpy(host, argv[i]);
            else
            {
                printf ("Invalid arguments\n");
                printf("Usage : %s --port <port number> --host <hostname> --file <filename>\n", argv[0]);
                return -1;
            }
        }
        else if(strcmp(argv[i], "--file")==0)
        {
            ++i;
            if(i<argc)
                strcpy(file, argv[i]);
            else
            {
                printf ("Invalid arguments\n");
                printf("Usage : %s --port <port number> --host <hostname> --file <filename>\n", argv[0]);
                return -1;
            }
        }
        else
        {
            printf ("Invalid arguments\n");
            printf("Usage : %s --port <port number> --host <hostname> --file <filename>\n", argv[0]);
            return -1;
            
        }
    }
    
    return 0;
}

/**********************************************************************/

int main(int argc, char *argv[])
{
    struct metrics metric;
    int fd;
    int len;
    struct addrinfo hints, *serveraddr;
    int result;
    char host[] = "127.0.0.1";
    char filename[128] = "despicableme-134.mp4";
    char port[6] = "8808";
    
    char ch = 'A';
    /* Check for hostname parameter */
    if (argc > 1) {
        if((check_arguments(argc, argv, port, host, filename))<0)
            return(0);
    }
    

    
    /* Lookup hostname */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &serveraddr) != 0) {
        printf("Hostname lookup failed\n");
        return 2;
    }
    
    /* Create a socket */
    if ((fd = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol)) == -1) {
        printf("Unable to create socket\n");
        return 3;
    }
    
    /* Connect to the receiver */
    if (connect(fd, serveraddr->ai_addr, serveraddr->ai_addrlen) != 0) {
        printf("Unable to connect to receiver\n");
        close(fd);
        return 4;
    }
    
    metric.sock = fd;
    metric.fptr=fopen(filename,"wb");
    if (metric.fptr==NULL)
    {
        perror ("Error opening file:");
        close(fd);
        return 6;
    }
    
    send_get_request(fd, host, filename);
    if(receive_response(fd, &metric)==0)
        printf("Successfully received file\n");
    
    fclose(metric.fptr);
    close(fd);
    return 0;
}

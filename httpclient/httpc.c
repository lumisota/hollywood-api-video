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
#include "readmpd.h"
#include "http_ops.h"

#define PAGESIZE 500000



#define ISspace(x) isspace((int)(x))
/**********************************************************************/

int receive_video_over_hlywd (int fd, struct metrics * metric)
{
    printf("Receiving file over TCP Hollywood\n");
    metric->Hollywood = 1;
    /* Create a Hollywood socket */
    /*TODO: What is the third argument?, need to make it user assigned*/
    if (hollywood_socket(fd, &(metric->h_sock), 0, 0) != 0) {
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

int check_arguments(int argc, char* argv[], char * port, char * mpdlink, char * filename)
{
    int i;
    for(i=1; i<argc; i++)
    {
        if(strcmp(argv[i], "--port")==0)
        {
            ++i;
            if(i<argc)
                strcpy(port, argv[i]);
            else
            {
                printf ("Invalid arguments\n");
                printf("Usage : %s --port <port number> --mpd <mpd link/url> --out <output file>\n", argv[0]);
                return -1;
            }
        }
        else if(strcmp(argv[i], "--mpd")==0)
        {
            ++i;
            if(i<argc)
                strcpy(mpdlink, argv[i]);
            else
            {
                printf ("Invalid arguments\n");
                printf("Usage : %s --port <port number> --mpd <mpd link/url> --out <output file>\n", argv[0]);
                return -1;
            }
        }
        else if(strcmp(argv[i], "--out")==0)
        {
            ++i;
            if(i<argc)
                strcpy(filename, argv[i]);
            else
            {
                printf ("Invalid arguments\n");
                printf("Usage : %s --port <port number> --mpd <mpd link/url> --out <output file>\n", argv[0]);
                return -1;
            }
        }
        else
        {
            printf ("Invalid arguments\n");
            printf("Usage : %s --port <port number> --mpd <mpd link/url> --out <output file>\n", argv[0]);
            return -1;
            
        }
    }
    
    return 0;
}

/**********************************************************************/

int fetch_manifest(int sockfd, char * mpdlink, struct metrics * metric)
{
    char buf[1024];
    char memory[PAGESIZE];
    int contentlen;
    
    send_get_request(sockfd, mpdlink, metric->Hollywood);

    get_html_headers(sockfd, buf, 1024, metric->Hollywood);
    
    if(strstr(buf, "200 OK")==NULL)
    {
        printf("Request Failed: \n");
        printf("%s",buf);
        return -1;
    }
    
    contentlen = get_content_length(buf);
    
    if (contentlen > PAGESIZE)
        printf("Manifest file larger than alotted memory\n");
    else if (contentlen == 0)
        printf("Received no content length for manifest file \n");


    if(write_to_memory (sockfd, memory, contentlen, metric->Hollywood)==0)
    {
        printf("Unable to receive mpd file \n");
        return -1;
    }
    else
        return read_mpddata(memory, mpdlink, metric);
    
    return -1;

}

/**********************************************************************/

int main(int argc, char *argv[])
{
    struct metrics metric;
    int fd;
    int len;
    struct addrinfo hints, *serveraddr;
    int result;
    char mpdlink[MAXURLLENGTH] = "www.itec.uni-klu.ac.at/ftp/datasets/DASHDataset2014/BigBuckBunny/4sec/BigBuckBunny_4s_simple_2014_05_09.mpd";
    char filename[128] = "output.ts";
    char port[6] = "8808";
    char path[380]  = "";
    char host[128]  = "";
    

    
    char ch = 'A';
    /* Check for hostname parameter */
    if (argc > 1) {
        if((check_arguments(argc, argv, port, mpdlink, filename))<0)
            return(0);
    }
    init_metrics(&metric);
    
 //   printf("Looking up host :%s port: %s\n", mpdlink, port );

    if (separate_host_and_filepath (mpdlink, host, path)<0)
    {
        printf ("Unable to separate link and filepath of the MPD link\n");
        return -1;
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
        return 5;
    }
 
    metric.stime = gettimelong();

    if(fetch_manifest(fd, mpdlink, &metric)<0)
        return 6;
    
    for (int i = 0; i < metric.num_of_levels ; i++)
    {
        printf(" BITRATE LEVEL : %d Presenting 1st and last URL\n", metric.bitrate_level[i].bitrate);
        printf(" %s\n", metric.bitrate_level[i].segments[0]);
        printf(" %s\n", metric.bitrate_level[i].segments[metric.num_of_segments-1]);
    }
    
    fclose(metric.fptr);
    close(fd);
    return 0;
}

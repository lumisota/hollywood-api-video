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
#include "../common/http_ops.h"
#include "mm_download.h"

#define ISspace(x) isspace((int)(x))
/**********************************************************************/
//
//int receive_video_over_hlywd (int fd, struct metrics * metric)
//{
//    printf("Receiving file over TCP Hollywood\n");
//    metric->Hollywood = 1;
//    /* Create a Hollywood socket */
//    /*TODO: What is the third argument?, need to make it user assigned*/
//    if (hollywood_socket(fd, &(metric->h_sock), 0, 0) != 0) {
//        printf("Unable to create Hollywood socket\n");
//        return -1;
//    }
//    
//    return (mm_parser(metric));
//    
//}
//
///**********************************************************************/
//
//int receive_video_over_tcp (int fd, struct metrics * metric)
//{
//    printf("Receiving file over TCP\n");
//    metric->Hollywood=0;
//    return (mm_parser(metric));
//}
/**********************************************************************/

int check_arguments(int argc, char* argv[], char * port, char * mpdlink, char * filename, uint8_t * hollywood)
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
        else if(strcmp(argv[i], "--hollywood")==0)
            *hollywood=1;
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

int fetch_manifest(transport * t, char * mpdlink, manifest * media_manifest )
{
    char buf[HTTPHEADERLEN];
    char memory[PAGESIZE];
    int contentlen;
    void * sock;
    printf("Hollywood is %d\n", t->Hollywood);
    
    if ( t->Hollywood)
    {
        sock = &(t->h_sock);
    }
    else
    {
        sock = &(t->sock);
    }
    
    send_get_request(sock, mpdlink, t->Hollywood);
        
    get_html_headers(sock, buf, HTTPHEADERLEN, t->Hollywood);
    
    
    if(strstr(buf, "200 OK")==NULL)
    {
        printf("Manifest Request Failed: \n");
        printf("%s",buf);
        return -1;
    }
    
    contentlen = get_content_length(buf);
    
    if (contentlen > PAGESIZE)
        printf("Manifest file larger than alotted memory\n");
    else if (contentlen == 0)
        printf("Received no content length for manifest file \n");

    if(read_to_memory (sock, memory, contentlen, t->Hollywood)==0)
    {
        printf("Unable to receive mpd file \n");
        return -1;
    }
    else
        return read_mpddata(memory, mpdlink, media_manifest);
    
    return -1;

}

/**********************************************************************/

int main(int argc, char *argv[])
{
    struct metrics metric;
    manifest media_manifest     ={0};
    transport media_transport;
    uint8_t hollywood = 0;
    char mpdlink[MAXURLLENGTH] = "127.0.0.1/BigBuckBunny/4sec/BigBuckBunny_4s_simple_2014_05_09.mpd";
    char filename[128] = "output.ts";
    char path[380]  = "";
    init_metrics(&metric);
    init_transport(&media_transport);

    /* Check for hostname parameter */
    if (argc > 1) {
        if((check_arguments(argc, argv, media_transport.port, mpdlink, filename, &hollywood))<0)
            return(0);
    }

    
 //   printf("Looking up host :%s port: %s\n", mpdlink, port );

    if (separate_host_and_filepath (mpdlink, media_transport.host, path)<0)
    {
        printf ("Unable to separate link and filepath of the MPD link\n");
        return -1;
    }
    if(hollywood)
    {
        if((media_transport.sock = connect_tcp_port (media_transport.host, media_transport.port, hollywood, &media_transport.h_sock))<0)
            return -1;
    }
    else
    {
        if((media_transport.sock = connect_tcp_port (media_transport.host, media_transport.port, hollywood, &media_transport.sock ))<0)
            return -1;
    }

    media_transport.fptr=fopen( filename, "wb" );

    if (media_transport.fptr==NULL)
    {
        perror ( "Error opening file:" );
        close(media_transport.sock);
        return 5;
    }
 
    media_transport.Hollywood = hollywood;
    metric.stime = gettimelong();
    metric.t = &media_transport;

    if(fetch_manifest(&media_transport, mpdlink, &media_manifest)<0)
        return 6;
    
    for (int i = 0; i < media_manifest.num_of_levels ; i++)
    {
        printf(" BITRATE LEVEL : %d Presenting 1st and last URL\n", media_manifest.bitrate_level[i].bitrate);
        printf(" %s\n", media_manifest.bitrate_level[i].segments[0]);
        printf(" %s\n", media_manifest.bitrate_level[i].segments[media_manifest.num_of_segments-1]);
    }
    
    if(play_video(&metric, &media_manifest, &media_transport)==0)
        printmetric(metric); 
    
    fclose(media_transport.fptr);
    close(media_transport.sock);
    return 0;
}

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
#include <signal.h>
#include "mm_parser.h"
#include "readmpd.h"
#include "../common/http_ops.h"
#include "mm_download.h"

extern int verbose;
int endnow = 0;
int buffer_dur_ms = DEFAULT_BUFFER_DURATION;
float min_rxcontent_ratio = 0.99;
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

void print_instructions(char * prog)
{
    printf("Usage : %s --port <port number> --mpd <mpd link/url> --out <output file> [--verbose] [--oo] [--prebuf x (ms)] [--bufferlen x (ms)] [--algo <panda|bola|abma] [--minrxration <0-1>]\n", prog);
    printf("Default paramaters: \nAlgo: BOLA\nProtocol: TCP\nLink: 127.0.0.1/BigBuckBunny/1sec/mp2s/BBB.mpd\nPort: 8080\nOutput: output.ts\n");
    printf("Min Rx Ratio: 0.8 (Ratio of Contenlen that must be received before requesting next chunk)\n");
}


int check_arguments(int argc, char* argv[], char * port, char * mpdlink, char * filename, uint8_t * hollywood, uint8_t * OO, int * prebuf, int * algo)
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
                print_instructions(argv[0]);
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
                print_instructions(argv[0]);
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
                print_instructions(argv[0]);
                return -1;
            }
        }
        else if(strcmp(argv[i], "--hollywood")==0)
            *hollywood=1;
        else if(strcmp(argv[i], "--prebuf")==0)
        {
            ++i; 
            *prebuf = atoi(argv[i]);
        }
        else if(strcmp(argv[i], "--bufferlen")==0)
        {
            ++i; 
            buffer_dur_ms = atoi(argv[i]);
            if(buffer_dur_ms < 1000)
            {
                printf("Invalid bufferlen argument, value in ms, must be greater than 1000ms\n"); 
                print_instructions(argv[0]); 
                return -1; 
            }
        }
        else if(strcmp(argv[i], "--minrxratio")==0)
        {
            ++i;
            min_rxcontent_ratio = atof(argv[i]);
            if(min_rxcontent_ratio < 0 || min_rxcontent_ratio > 1)
            {
                printf("Invalid minrxratio argument, value must be between 0 and 1\n");
                print_instructions(argv[0]);
                return -1;
            }
        }

        else if(strcmp(argv[i], "--oo")==0)
            *OO=1;
        else if(strcmp(argv[i], "--verbose")==0)
            verbose = 1;
        else if(strcmp(argv[i], "--algo")==0)
        {
            ++i;
            if(i<argc)
            {
                if(strcmp(argv[i], "bola")==0){
                    *algo = 0;
                }
                else if(strcmp(argv[i], "panda")==0){
                    *algo = 1;
                }
                else if(strcmp(argv[i], "abma")==0){
                    *algo = 2;
                }
                else
                {
                    printf ("Invalid arguments\n");
                    print_instructions(argv[0]);
                    return -1;
                }
            }
            else
            {
                printf ("Invalid arguments\n");
                print_instructions(argv[0]);
                return -1;
            }
        }

        else
        {
            printf ("Invalid arguments\n");
            print_instructions(argv[0]);
            return -1;
            
        }
    }
    
    return 0;
}

/**********************************************************************/

long fetch_manifest(transport * t, char * mpdlink, manifest * media_manifest )
{
    char buf[HTTPHEADERLEN];
    char memory[PAGESIZE];
    int contentlen;
    void * sock;
    uint64_t start_time;
    long Throughput = -1;
    uint8_t substream;
    
    if ( t->Hollywood)
    {
        sock = &(t->h_sock);
    }
    else
    {
        sock = &(t->sock);
    }
    start_time = gettimelong();
    send_get_request(sock, mpdlink, t->Hollywood, 0);
    if(get_html_headers(sock, buf, HTTPHEADERLEN, t->Hollywood, &substream, NULL, NULL)<=0)
    {
        printf("Error: Received no GET response from server\n");
        return -1;
    }
    t->rtt = gettimelong() - start_time;

    if (substream == HOLLYWOOD_DATA_SUBSTREAM_TIMELINED || substream == HOLLYWOOD_DATA_SUBSTREAM_UNTIMELINED)
    {
        printf("Error: Received timelined message in manifest GET\n");
        return -1; 
    }
    
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

    t->bytes_rx = read_to_memory (sock, memory, contentlen, t->Hollywood);
    if(t->bytes_rx<=0)
    {
        printf("Unable to receive mpd file \n");
        return -1;
    }
    else
    {
       // double download_time = gettimelong() - start_time;
        t->download_time =gettimelong() - start_time;
        Throughput = (long)((double)t->bytes_rx*8000000/(double)t->download_time);  /*bps*/
        if(read_mpddata(memory, mpdlink, media_manifest)<0)
        {
            printf("Unable to parse mpd file\n");
            return -1;
        }
        return Throughput;
    }
    

    return -1;

}

/**********************************************************************/

static void signal_handler (int signo)
{
    endnow = 1;
}
/**********************************************************************/

static int prepare_exit()
{
    if(signal(SIGINT, signal_handler) == SIG_ERR)
        return -1; 
   
    if(signal(SIGTERM, signal_handler) == SIG_ERR)
        return -1;

    return 0;
} 

/**********************************************************************/

int main(int argc, char *argv[])
{
    struct metrics metric;
    manifest media_manifest     = {0};
    transport media_transport;
    uint8_t hollywood = 0, oo = 0;
    char mpdlink[MAXURLLENGTH] = "127.0.0.1/BigBuckBunny/1sec/mp2s/BBB.mpd";
    char filename[128] = "output.ts";
    char path[380]  = "";
    int prebuf = MIN_PREBUFFER; 
    long initial_throughput;
    if (prepare_exit()<0)
    {
        printf("Signal handlers failed to register\n"); 
        return 0; 
    }

    init_metrics(&metric);

    init_transport(&media_transport);

    /* Check for hostname parameter */
    if (argc > 1) {
        if((check_arguments(argc, argv, media_transport.port, mpdlink, filename, &hollywood, &oo, &prebuf, & media_transport.algo))<0)
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
        if((media_transport.sock = connect_tcp_port (media_transport.host, media_transport.port, hollywood, &media_transport.h_sock, oo))<0)
            return -1;
    }
    else
    {
        if((media_transport.sock = connect_tcp_port (media_transport.host, media_transport.port, hollywood, &media_transport.sock, 0))<0)
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
    media_transport.OO = oo;
    metric.stime = gettimelong();
    metric.t = &media_transport;
    metric.minbufferlen = prebuf;
     

    initialize_http_operations(metric.stime);

    initial_throughput = fetch_manifest(&media_transport, mpdlink, &media_manifest);
    if(initial_throughput<0)
    {
        printf("received negative throughput\n");
        return 6;
    }
    for (int i = 0; i < media_manifest.num_of_levels ; i++)
    {
        printdebug(READMPD, " BITRATE LEVEL : %d (%d) Presenting 1st and last URL\n", media_manifest.bitrate_level[i].bitrate, i);
        printdebug(READMPD, " %s\n", media_manifest.bitrate_level[i].segments[0]);
        printdebug(READMPD, " %s\n", media_manifest.bitrate_level[i].segments[media_manifest.num_of_segments-1]);
    }
    if(play_video(&metric, &media_manifest, &media_transport, initial_throughput)==0)
        printmetric(metric, media_transport);
    
    exit_http_operations(); 
    fclose(media_transport.fptr);
    close(media_transport.sock);
    return 0;
}

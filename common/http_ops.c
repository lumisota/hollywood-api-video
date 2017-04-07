//
//  http_ops.c
//  
//
//  Created by Saba Ahsan on 21/02/17.
//
//

#include "http_ops.h"



/**********************************************************************/


int connect_tcp_port (char * host, char * port, uint8_t hollywood, void * sock)
{
    struct addrinfo hints;
    struct addrinfo *serveraddr;
    int fd;
    
    /* Lookup hostname */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &serveraddr) != 0) {
        printf("Hostname lookup failed\n");
        return -1;
    }
    
    /* Create a socket */
    if ((fd = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol)) == -1) {
        printf("Unable to create socket\n");
        return -1;
    }
    
    /* Connect to the receiver */
    if (connect(fd, serveraddr->ai_addr, serveraddr->ai_addrlen) != 0) {
        printf("Unable to connect to receiver\n");
        close(fd);
        return -1;
    }
    if (hollywood)
    {
        /* Create Hollywood socket */
        if (hollywood_socket(fd, (hlywd_sock *) sock, 0, 0) != 0) {
            printf("Unable to create Hollywood socket\n");
            close(fd);
            return -1;
        }
        
    }
    else
    {
        *(int *)sock = fd;
    }
        
    return fd;
}

/**********************************************************************/

int get_content_length( char * buf)
{
    char * tmp;
    
    if((tmp = strstr(buf, "Content-Length:")) != NULL)
        return atoi (tmp+15);
    else
        return 0;
}


/**********************************************************************/

int get_html_headers(void * sock, char *buf, int size, uint8_t hollywood)
{
    int i = 0;
    char c = '\0';
    int n;
    
    if(hollywood)
    {
        /*headers are sent as a single message*/
        uint8_t substream_id;
        i = recv_message((hlywd_sock * )sock, buf, size, 0, &substream_id);
        if ( i < 0 )
        {
            printf("get_html_headers: Error reading from Hollywood socket \n");
            i = 0;
        }
    }
    else
    {
        while (i < size - 1)
        {
            n = recv(*((int *)sock), &c, 1, 0);;
           // printf("%02X: %d\n", c, n); fflush(stdout);
            if (n > 0)
            {
                buf[i] = c;
                i++;
            }
            else if (n==0)
            {
              //  printf("get_html_headers: socket disconnected\n");
                break;
            }
            else
            {
                perror("get_html_headers: an error occured when receiving on TCP \n");
                break;
            }
            
            if (i >= 4)
                if (strncmp(buf+i-4,"\r\n\r\n",4)==0)
                    break;
        }
   // printf("get_html_headers: Read n bytes: %d", i);
    }
    buf[i] = '\0';
   // printf("Received response : %s\n", buf);
    return(i);
}


/**********************************************************************/

int send_get_request(void * sock, char * url, uint8_t hollywood)
{
    char request[HTTPHEADERLEN]   = "";
    char host[MAXHOSTLEN]      = "";
    char filename[MAXURLLENGTH - MAXHOSTLEN]     = "";
    
    if(separate_host_and_filepath( url, host, filename)<0)
    {
        printf("Unable to separate host and file path from link\n");
        return -1;
    }

    
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n\r\n", filename, host);
//    printf("%s\nLENGTH: %d\n", request, strlen(request));
    if(hollywood)
    {
        return send_message((hlywd_sock *)sock, request, strlen(request), 0);
    }
    else
    {
        return write(*((int *) sock), request, strlen(request));

    }
}

/**********************************************************************/

int separate_host_and_filepath(char * url, char * host, char * path)
{
    uint8_t url_found   = 0;
    int url_len         = strlen (url);
    int i; 
    
    
    for (i = 0; i < url_len ; i++ )
    {
        if (url_found==0)
        {
            if ( url[i] == '.' )
                url_found = 1;
        }
        else
        {
            if ( url[i] == '/' )
            {
                memcpy(host, url, i);
                memcpy(path, url+i, url_len-i);
                host[i] = '\0';
                path[url_len - i] = '\0';
                return 0;
            }
        }
    }
    return -1;
    
}

/**********************************************************************/


int read_http_body_partial(void * sock, uint8_t * buf, int buflen, uint8_t hollywood, uint32_t * seq, uint32_t * offset)
{
    int ret;
    if(hollywood)
    {
        uint8_t substream_id;
     //   printf("http_read:  "); fflush(stdout);
        ret = recv_message((hlywd_sock * )sock, buf, buflen, 0, &substream_id);
       // printf("Read %d bytes\n",ret); fflush(stdout);
        if (ret > 0)
        {
            ret -= HLYWD_MSG_TRAILER;
            if(offset!=NULL)
            {
                memcpy(offset, buf+ret, sizeof(uint32_t));
                *offset = ntohl(*offset);
            }
            if(seq!=NULL)
            {
                memcpy(seq, buf+ret+sizeof(uint32_t), sizeof(uint32_t));
                *seq = ntohl(*seq);
            }
        }
    }
    else
    {
        ret = recv(*((int *)sock), buf, buflen, 0);
        if(seq!=NULL)
        {
            *seq = *seq+1;
        }
    }
    
    return ret;
}

/**********************************************************************/


int read_to_memory (void * sock, char * memory, int contentlen, uint8_t hollywood)
{
    int ret             = 0;
    int bytes_written   = 0;
//    printf("Content len: %d ......", contentlen );
    do
    {
        
        if(hollywood)
        {
            uint8_t substream_id;
            ret = recv_message((hlywd_sock * )sock, memory + bytes_written, contentlen - bytes_written, 0, &substream_id);
        }
        else
        {
            ret = recv(*((int *)sock), memory + bytes_written, contentlen - bytes_written, 0);
        }

        if ( ret > 0 )
            bytes_written += ret;
        else
        {
            printf("read_to_memory: Received return value %d while reading socket\n", ret);
        }
    }while(ret > 0 && bytes_written < contentlen);
    
 //   printf(" received\n");
    
    return bytes_written;
}


/**************** SERVER HTTP ****************************************/

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/**********************************************************************/
int send_resp_headers(void * sock , const char *filename, uint8_t hollywood)
{
    char buf[HTTPHEADERLEN];
    char tmp[256];
    struct stat st;
    int size;
    
    
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    
    strcat(buf, SERVER_STRING);
    
    stat(filename, &st);
    size = st.st_size;
    sprintf(tmp, "Content-Length: %d\r\n", size);
    
    strcat(buf, tmp);
    
    
    if(strstr(filename, "mp4")!=NULL)
        strcat(buf, "Content-Type: video/mp4\r\n");
    else if(strstr(filename, "m4s")!=NULL)
        strcat(buf, "Content-Type: video/mp4\r\n");
    else if(strstr(filename, "ts")!=NULL)
        strcat(buf, "Content-Type: video/m2ts\r\n");
    else if(strstr(filename, "mpd")!=NULL)
        strcat(buf, "Content-Type: xml/manifest\r\n");
    else
        strcat(buf, "Content-Type: unknown\r\n");
    
    strcat(buf, "\r\n");
    
    if(hollywood)
    {
        return send_message((hlywd_sock *)sock, buf, strlen(buf), 0);
    }
    else{
        return write(*((int *) sock), buf, strlen(buf));
    }
    
}


/**********************************************************************/
int cat_full_file(hlywd_sock * sock, FILE *fptr)
{
    char buffer[PAGESIZE];
    int bytes_read;
    
    bytes_read = fread(buffer, sizeof(char), PAGESIZE, fptr);
    
    if ( feof(fptr) )
    {
        return send_message(sock, buffer, bytes_read, 0);
    }
    
    if(bytes_read == PAGESIZE)
    {
        printf("Couldn't read the entire file contents: file too big \n");
    }
    else
    {
        printf("An error occurred while reading the file for sending\n");
    }
    
    return -1; 
    
}

/**********************************************************************/
int cat_stream(int client, FILE *fptr)
{
    char    buffer[1024];
    int     bytes_read, msg_len;
    
    bytes_read = fread(buffer, 1, 1024, fptr);
    while(bytes_read==1024) {
        msg_len = send(client, buffer, bytes_read, 0);
        if (msg_len == -1) {
            printf("Unable to send message over TCP\n");
            return -1;
        }
        bytes_read = fread(buffer, 1,1024,fptr);
        
    }
    if (feof(fptr)) {
        if(bytes_read > 0)
        {
            msg_len = send(client, buffer, bytes_read, 0);
        }
        return 0;
    }
    else
    {
        printf ("An error occured while reading the file.\n");
        return -1;
    }
    
}



/**********************************************************************/

int cat(void * sock, FILE * fptr, uint8_t hollywood)
{
    if (hollywood)
    {
        return cat_full_file((hlywd_sock *)sock, fptr);
    }
    else
    {
        return cat_stream(*((int *)sock), fptr);
    }
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
int not_found(void * sock, uint8_t hollywood)
{
    char buf[HTTPHEADERLEN];
    
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    strcat(buf, SERVER_STRING);
    strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "\r\n");
    strcat(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    strcat(buf, "<BODY><P>The server could not fulfill\r\n");
    strcat(buf, "your request because the resource specified\r\n");
    strcat(buf, "is unavailable or nonexistent.\r\n");
    strcat(buf, "</BODY></HTML>\r\n");
    
    if ( hollywood )
    {
        return send_message(sock, buf, strlen(buf), 0);
    }
    else
    {
        return send(*((int *)sock), buf, strlen(buf), 0);
    }
}


/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
int unimplemented(void * sock, uint8_t hollywood)
{
    char buf[HTTPHEADERLEN];
    
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    strcat(buf, SERVER_STRING);
    strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "\r\n");
    strcat(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    strcat(buf, "</TITLE></HEAD>\r\n");
    strcat(buf, "<BODY><P>HTTP request method not supported.\r\n");
    strcat(buf, "</BODY></HTML>\r\n");
    
    if ( hollywood )
    {
        return send_message(sock, buf, strlen(buf), 0);
    }
    else
    {
        return send(*((int *)sock), buf, strlen(buf), 0);
    }
}




//
//  http_ops.c
//  
//
//  Created by Saba Ahsan on 21/02/17.
//
//

#include "http_ops.h"


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

int get_html_headers(int sock, char *buf, int size, uint8_t hollywood)
{
    int i = 0;
    char c = '\0';
    char * tmp;
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
/*
int receive_response(int fd, struct metrics * metric, uint8_t hollywood)
{
    char buf[1024];
    
    get_html_headers(fd, buf, 1024, hollywood);
    
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
    return -1;
}

 */


/**********************************************************************/

int send_get_request(int fd, char * url, uint8_t hollywood)
{
    char request[MAXREQUESTLENGTH]   = "";
    char host[MAXHOSTLEN]      = "";
    char filename[MAXURLLENGTH - MAXHOSTLEN]     = "";
    
    if(separate_host_and_filepath( url, host, filename)<0)
    {
        printf("Unable to separate host and file path from link\n");
        return -1;
    }

    
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n\r\n", filename, host);
    if(write(fd, request, strlen(request))<0)
    {
        printf("Failed to send GET request\n");
        return -1;
    }
    return 0;
}

/**********************************************************************/

int separate_host_and_filepath(char * url, char * host, char * path)
{
    uint8_t url_found   = 0;
    int url_len         = strlen (url);
    
    
    for (int i = 0; i < url_len ; i++ )
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

int write_to_memory (int sockfd, char * memory, int contentlen, uint8_t hollywood)
{
    int ret             = 0;
    int bytes_written   = 0;
    do
    {
        ret = recv(sockfd, memory + bytes_written, contentlen - bytes_written, 0);
        bytes_written += ret;
    }while(ret>0 && bytes_written < contentlen);
    
    return bytes_written;
}




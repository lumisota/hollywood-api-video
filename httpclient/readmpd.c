//
//  readmpd.c
//  
//
//  Created by Saba Ahsan on 08/11/16.
//
//

#include "readmpd.h"


float get_duration(char * dur )
{
    float hour, min, sec;
    char * tmp2, *tmp3;
    tmp3=dur;
    tmp2 = strstr(tmp3, "H");
    if(tmp2==NULL)
    hour=0.0;
    else
    {
        *tmp2='\0';
        hour = atof(tmp3+2);
        tmp3 = tmp2+1;
    }
    
    tmp2 = strstr(tmp3, "M");
    if(tmp2==NULL)
    min=0.0;
    else
    {
        *tmp2='\0';
        min = atof(tmp3);
        tmp3 = tmp2+1;
    }
    
    tmp2 = strstr(tmp3, "S");
    if(tmp2==NULL)
    sec=0.0;
    else
    {
        *tmp2='\0';
        sec = atof(tmp3);
        tmp3 = tmp2+1;
    }
    return (hour*3600)+(min*60)+sec;
}


/*return length of the new link or 0 if unsuccessful*/

int get_base_url (char * link)
{
    for ( int i = strlen(link) - 1; i >= 0; i --)
    {
        if ( link[i] == '/' )
        {
            link[i+1] = '\0';
            return strlen(link);
        }
    }
    return 0;
}

int read_mpddata(char * memory, char mpdlink[], manifest * m)
{
    xmlAttr         * attribute;
    xmlDoc          *document;
    xmlNode         *root, *first_child, *node, *second_child, *node2, *third_child, *node3, *fourth_child, *node4;
    char            duration[25]="\0";
    float           dur, segdur=0, timescale=0;
    int             num_of_rates = 0, height;
    char            bw[25] = "", repid[25] = "";
    char            bandwidth[MAX_SUPPORTED_BITRATE_LEVELS][25];
    char            id[MAX_SUPPORTED_BITRATE_LEVELS][5] = {{0}};
    char *          tmp, * newurl;
    char            keyword_id [12] = "$id$";
    char            keyword_bw [12] = "$Bandwidth$";
    char            keyword_num [12] = "$Number$";
    char            segnum[5];
    char            base_url[MAXURLLENGTH]  = "";
    char            init_url_template[MAXURLLENGTH] = "";
    char            media_url_template[MAXURLLENGTH] = "";
    char            init_url[MAX_SUPPORTED_BITRATE_LEVELS][MAXURLLENGTH]  = {""};
    char            media_url[MAX_SUPPORTED_BITRATE_LEVELS][MAXURLLENGTH] = {""};
    int             startNumber[MAX_SUPPORTED_BITRATE_LEVELS], sn = 1; 
    
    
    strcpy(base_url, mpdlink);
    if(get_base_url(base_url)==0)
    {
        printdebug(READMPD,"Unable to successfully extract the base URL\n");
        return -1;
    }
    
    document = xmlReadMemory(memory, strlen(memory), mpdlink, NULL, 0);
    root = xmlDocGetRootElement(document);
//    fprintf(stdout, "Root is <%s> (%i)\n", root->name, root->type);
    
    first_child = root->children;
    for (node = first_child; node; node = node->next)
    {
//        fprintf(stdout, "\t Child is <%s> (%i)\n", node->name, node->type);
        if(xmlStrcmp(node->name, (const xmlChar *) "Period")==0)
        {
            attribute = node->properties;
            while(attribute)
            {
                if(xmlStrcmp(attribute->name, (const xmlChar *) "duration")==0)
                {
                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                    printdebug(READMPD,"\t\t%s : %s\n",(char *)attribute->name, (char *)value);
                    strcpy(duration, (char *)value);
                    printdebug(READMPD, "Duration is %s and %f\n", duration, dur = get_duration(duration));
                    get_duration(duration);
                    xmlFree(value);
                    break;
                }
                attribute = attribute->next;
            }
            second_child = node->children;
            for (node2 = second_child; node2; node2 = node2->next)
            {
             //   fprintf(stdout, "\t\t Child is <%s> (%i)\n", node2->name, node2->type);
                if(xmlStrcmp(node2->name, (const xmlChar *) "AdaptationSet")==0)
                {
                    third_child = node2->children;
                    for (node3 = third_child; node3; node3 = node3->next)
//                    {
//                        if(ret>0 && xmlStrcmp(node3->name, (const xmlChar *) "Representation"))
//                        {
//                            /* add 2 to include init segment*/
//                            m->num_of_segments = ceil(dur/(segdur/timescale)) + 1;
//                            printdebug(READMPD, "Init segment : %s\n", init_url[num_of_rates]);
//                            printdebug(READMPD, "Timescale : %f, Seg duration : %f\n", timescale, segdur);
//                            printdebug(READMPD, "Number of Segments = %d\n", m->num_of_segments);
//                            
//                            strcpy(bandwidth[num_of_rates], bw);
//                            strcpy(id[num_of_rates], repid);
//                            num_of_rates++;
//                            if(num_of_rates>=MAX_SUPPORTED_BITRATE_LEVELS)
//                            {
//                                printdebug(READMPD,"Number of rate levels exceeds the maximum allowed value\n");
//                                return -1;
//                            }
//                            ret = 0;
//                            strcpy(media_url[num_of_rates], base_url);
//                            strcpy(init_url[num_of_rates], base_url);
//                        }
//                        ret = populate_attributes(node, node3, m);
//                        if(ret<0)
//                            return -1;
//                        fourth_child = node3->children;
//                        for (node4 = fourth_child; node4; node4 = node4->next)
//                        {
//                            if(populate_attributes(node, node4, m)<0)
//                                return -1;
//                        }
//                           
//                        
//                    }
//                    
//                    if(ret>0)
//                    {
//                        /* add 2 to include init segment*/
//                        m->num_of_segments = ceil(dur/(segdur/timescale)) + 1;
//                        printdebug(READMPD, "Init segment : %s\n", init_url[num_of_rates]);
//                        printdebug(READMPD, "Timescale : %f, Seg duration : %f\n", timescale, segdur);
//                        printdebug(READMPD, "Number of Segments = %d\n", m->num_of_segments);
//                      
//                        strcpy(bandwidth[num_of_rates], bw);
//                        strcpy(id[num_of_rates], repid);
//                        num_of_rates++;
//                        if(num_of_rates>=MAX_SUPPORTED_BITRATE_LEVELS)
//                        {
//                            printdebug(READMPD,"Number of rate levels exceeds the maximum allowed value\n");
//                            return -1;
//                        }
//                    }
//                  


                    {
                        if(xmlStrcmp(node3->name, (const xmlChar *) "SegmentTemplate")==0)
                        {
                            printdebug(READMPD, "\t\t Child is <%s> (%i)\n", node3->name, node3->type);
                            attribute = node3->properties;
                            while(attribute)
                            {
                                printdebug(READMPD,">>>>>>>>>>>>>>>>%s\n", (char *)attribute->name);
                                if(xmlStrcmp(attribute->name, (const xmlChar *) "duration")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    segdur=atoi((char *)value);
                                    xmlFree(value);
                                }
                                else if(xmlStrcmp(attribute->name, (const xmlChar *) "timescale")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    timescale=atoi((char *)value);
                                    xmlFree(value);
                                }
                                else if(xmlStrcmp(attribute->name, (const xmlChar *) "startNumber")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    sn=atoi((char *)value);
                                    xmlFree(value);
                                }
                                else if(xmlStrcmp(attribute->name, (const xmlChar *) "initialization")==0
                                        || xmlStrcmp(attribute->name, (const xmlChar *) "index")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    strcpy(init_url_template, base_url);
                                    strcat(init_url_template, (char *)value);
                                    printdebug(READMPD,"Init_url being filled here!!! %s (%s)\n", init_url_template, (char *)value); fflush(stdout);
                                    xmlFree(value);
                                }
                                else if(xmlStrcmp(attribute->name, (const xmlChar *) "media")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    strcpy(media_url_template, base_url);
                                    strcat(media_url_template, (char *) value);
                                    printdebug(READMPD,"Media_url is %s \n", media_url_template);
                                    xmlFree(value);
                                }
                                
                                attribute = attribute->next;
                            }
                            /* add 2 to include init segment*/
                            if (strlen(init_url_template)>0)
                    			m->init = 1; 
		                    else 
                                m->init = 0; 
                            m->num_of_segments = ceil(dur/(segdur/timescale)) + m->init;
                        }
                        
                        if(xmlStrcmp(node3->name, (const xmlChar *) "Representation")==0)
                        {
                    //        fprintf(stdout, "\t\t Child is <%s> (%i)\n", node3->name, node3->type);

                            attribute = node3->properties;
                            while(attribute)
                            {
                                if(xmlStrcmp(attribute->name, (const xmlChar *) "id")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    strcpy(repid,(char *)value);
                                    xmlFree(value);
                                }
                                else if(xmlStrcmp(attribute->name, (const xmlChar *) "height")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    height=atoi((char *)value);
                                    xmlFree(value);
                                }
                                else if(xmlStrcmp(attribute->name, (const xmlChar *) "bandwidth")==0)
                                {
                                    xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                    strcpy(bw,(char *)value);
                                    xmlFree(value);
                                }
                                
                                attribute = attribute->next;
                            }
                            
                            fourth_child = node3->children;
                            for (node4 = fourth_child; node4; node4 = node4->next)
                            {
                                if(xmlStrcmp(node4->name, (const xmlChar *) "SegmentTemplate")==0)
                                {
                                //    fprintf(stdout, "\t\t Child is <%s> (%i)\n", node4->name, node4->type);
                                    attribute = node4->properties;
                                    while(attribute)
                                    {
                                        printdebug(READMPD,">>>>>>>>>>>>>>>>%s\n", (char *)attribute->name);
                                        if(xmlStrcmp(attribute->name, (const xmlChar *) "duration")==0)
                                        {
                                            xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                            segdur=atoi((char *)value);
                                            xmlFree(value);
                                        }
                                        else if(xmlStrcmp(attribute->name, (const xmlChar *) "timescale")==0)
                                        {
                                            xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                            timescale=atoi((char *)value);
                                            xmlFree(value);
                                        }
                                        else if(xmlStrcmp(attribute->name, (const xmlChar *) "startNumber")==0)
                                        {
                                            xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                            sn=atoi((char *)value);
                                            xmlFree(value);
                                        }
                                        else if(xmlStrcmp(attribute->name, (const xmlChar *) "initialization")==0
                                                || xmlStrcmp(attribute->name, (const xmlChar *) "index")==0)
                                        {
                                            xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                            strcpy(init_url_template, base_url);
                                            strcat(init_url_template, (char *)value);
                                            printdebug(READMPD,"Init_url being filled here!!! %s (%s)\n", init_url_template, (char *)value); fflush(stdout);
                                            xmlFree(value);
                                        }
                                        else if(xmlStrcmp(attribute->name, (const xmlChar *) "media")==0)
                                        {
                                            xmlChar* value = xmlNodeListGetString(node->doc, attribute->children, 1);
                                            strcpy(media_url_template, base_url);
                                            strcat(media_url_template, (char *) value);
                                            printdebug(READMPD,"Media_url is %s \n", media_url_template);
                                            xmlFree(value);
                                        }
                                        
                                        attribute = attribute->next;
                                    }
                                    /* add 1 to include init segment*/
                                    if (strlen(init_url_template)>0)
                    					m->init = 1; 
				                    else 
                                        m->init = 0; 
                                    m->num_of_segments = ceil(dur/(segdur/timescale)) + m->init;

                                }
                            }
                            strcpy(media_url[num_of_rates], media_url_template);
                            strcpy(init_url[num_of_rates], init_url_template);
                            strcpy(bandwidth[num_of_rates], bw);
                            strcpy(id[num_of_rates], repid);
                            startNumber[num_of_rates] = sn; 
                            printdebug(READMPD, "Init segment : %s\n", init_url[num_of_rates]);
                            printdebug(READMPD, "Media segment : %s\n", media_url[num_of_rates]);
                            printdebug(READMPD, "Timescale : %f, Seg duration : %f\n", timescale, segdur);
                            printdebug(READMPD, "Number of Segments = %d\n", m->num_of_segments);
                            num_of_rates++;
                            if(num_of_rates>=MAX_SUPPORTED_BITRATE_LEVELS)
                            {
                                printdebug(READMPD,"Number of rate levels exceeds the maximum allowed value\n");
                                return -1;
                            }
                            
                        }
                        
                    }
            
                }
            }
            
        }

    }

    if( m->num_of_segments < 0)
    {
        printdebug(READMPD,"Number of rate levels / segments is negative, check mpd. \n");
        return -1;
    }
    
    m->num_of_levels = num_of_rates;
    m->segment_dur = (segdur/timescale);
    
    int j, k;
    for (j = 0; j < num_of_rates; j++)
    {
        level * next_level = &m->bitrate_level[j];
        next_level->segments = malloc (m->num_of_segments * sizeof(char *));
        
        for (k = 0; k < m->num_of_segments ; k++)
            next_level->segments[k] = malloc ( MAXURLLENGTH * sizeof (char));
        
        next_level->bitrate = atoi(bandwidth[j]);
        if( strlen(init_url[j])!=0)
	    {
            newurl = str_replace(init_url[j], keyword_bw, bandwidth[j]);
            if(newurl == NULL)
            {
                newurl = str_replace(init_url[j], keyword_id, id[j]);
                if(newurl == NULL)
                    strcpy(next_level->segments[0], init_url[j]);
                else
                {
                    strcpy(next_level->segments[0], newurl);
                    free(newurl);
                }
            }
            else
            {
                //printdebug(READMPD,"Replaced bandwidth \n");
                strcpy(next_level->segments[0], newurl);
                free(newurl);
            }
        }

	    sn = startNumber[j];
        for (k = m->init; k < m->num_of_segments; k++)
        {
            sprintf(segnum,"%d", sn);
            sn++; 
            tmp = str_replace(media_url[j], keyword_bw, bandwidth[j]);
            if ( tmp == NULL )
            {
                tmp = str_replace(media_url[j], keyword_id, id[j]);
                if(tmp == NULL)
                {
                    newurl = str_replace(media_url[j], keyword_num, segnum);
                    if(newurl == NULL)
                    {
                        strcpy(next_level->segments[k], media_url[j]);
                    }
                    else
                    {
                        strcpy(next_level->segments[k], newurl);
                        free(newurl);
                    }
                }
                else
                {
                    newurl = str_replace(tmp, keyword_num, segnum);
                    if(newurl == NULL)
                    {
                        strcpy(next_level->segments[k], tmp);
                    }
                    else
                    {
                        strcpy(next_level->segments[k], newurl);
                        free(newurl);
                    }
                    free(tmp);
                }
            }
            else
            {
                newurl = str_replace(tmp, keyword_num, segnum);
                if(newurl == NULL)
                {
                    strcpy(next_level->segments[k], tmp);
                }
                else
                {
                    strcpy(next_level->segments[k], newurl);
                    free(newurl);
                }
                free(tmp);
            }
            //printdebug(READMPD,"%s\n", next_level->segments[k]);

        }
    }

    
    return 0;

    
}

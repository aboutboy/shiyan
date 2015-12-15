/*
 * http_uri.c --- Contains functions to parse uri's
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 4-Jul-98
 *
 * Copyright (C) 1998 Free Software Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "http_uri.h"

typedef enum uri_parse_state_tag
{
  parse_state_read_host = 0,
  parse_state_read_port,
  parse_state_read_resource
} uri_parse_state;

/*2015.10.12  Add by wolf-lone*/
int http_uri_parse_2(char *a_string, http_uri *a_uri)
{
#define DEFAULT_PORT	0
#define check_null(ptr)	do{ if(ptr == NULL)	 goto ec;} while(0)

	char *str = NULL;
	char *host_s = NULL;
	char *source_s = NULL;
	char *port_s = NULL;
	char *param_s = NULL;
	int len = 0;
	char  tmp_port[6];
	
	check_null(a_string);
	if (a_uri) 
	{
		a_uri->full = strdup(a_string);
		if(a_uri->full == NULL)
			goto ec;
	}

	str = strchr(a_string, ':');
	check_null(str);
	if (strncmp(str, "://", 3) != 0)
		goto ec;

	if (a_uri) 
	{
		len = str - a_string;
		a_uri->proto = (char *)malloc(len + 1);
		memcpy(a_uri->proto, a_string, len);
		a_uri->proto[len] = '\0';
	}

	host_s = &str[3];
	if(host_s == NULL || strlen(host_s) == 0)
		goto ec;

	port_s = strchr(host_s, ':');
	source_s = strchr(host_s, '/');

	if(port_s == NULL && source_s == NULL)
	{
		if(a_uri)
		{
			a_uri->host = strdup(host_s);
			check_null(a_uri->host);
			a_uri->port = DEFAULT_PORT;
			
			a_uri->resource = strdup("/");
			check_null(a_uri->resource);
		}
		return 0;
	}
	
	if(port_s != NULL || source_s != NULL)
	{
		if(a_uri)
		{
			if(port_s != NULL)
				str = port_s;
			else
				str = source_s;
			
			len = str - host_s;
			a_uri->host = (char *)malloc(len + 1);
			check_null(a_uri->host);
			memcpy(a_uri->host, host_s, len);
			a_uri->host[len] = '\0';

			if(port_s != NULL)
			{
				len = source_s - port_s;
				if(len >= sizeof(tmp_port))
					goto ec;
				memcpy(tmp_port, port_s, len);
				tmp_port[len] = '\0';
				a_uri->port = atoi(tmp_port);
			}
			else
				a_uri->port = DEFAULT_PORT;
			
			if(source_s != NULL)
				a_uri->resource = strdup(source_s);
			else
				a_uri->resource = strdup("/");
			check_null(a_uri->resource);
		}
		return 0;
	}
ec:
	return -1;
}

int
http_uri_parse(char *a_string,
	       http_uri *a_uri)
{
#if 0
	return http_uri_parse_2(a_string, a_uri);
#else
  /* Everyone chant... "we love state machines..." */
  uri_parse_state l_state = parse_state_read_host;
  char *l_start_string = NULL;
  char *l_end_string = NULL;
  char  l_temp_port[6];

  /* init the array */
  memset(l_temp_port, 0, 6);
  /* check the parameters */
  if (a_string == NULL)
    goto ec;
  if (a_uri) {
    a_uri->full = strdup(a_string);
  }
  l_start_string = strchr(a_string, ':');
  /* check to make sure that there was a : in the string */
  if (!l_start_string)
    goto ec;
  if (a_uri) {
    a_uri->proto = (char *)malloc(l_start_string - a_string + 1);
    memcpy(a_uri->proto, a_string, (l_start_string - a_string));
    a_uri->proto[l_start_string - a_string] = '\0';
  }
  /* check to make sure it starts with "http://" */
  if (strncmp(l_start_string, "://", 3) != 0)
    goto ec;
  /* start at the beginning of the string */
  l_start_string = l_end_string = &l_start_string[3];
	while(*l_end_string)
	{
		if (l_state == parse_state_read_host)
		{
			if (*l_end_string == ':')
			{
				l_state = parse_state_read_port;
				if ((l_end_string - l_start_string) == 0)
					goto ec;
				/* allocate space */
				if ((l_end_string - l_start_string) == 0)
					goto ec;
				/* only do this if a uri was passed in */
				if (a_uri)
				{
					a_uri->host = (char *)malloc(l_end_string - l_start_string + 1);
					/* copy the data */
					memcpy(a_uri->host, l_start_string, (l_end_string - l_start_string));
					/* terminate */
					a_uri->host[l_end_string - l_start_string] = '\0';
				}
				/* reset the counters */
				l_end_string++;
				l_start_string = l_end_string;
				continue;
			}
			else if (*l_end_string == '/')
			{
				l_state = parse_state_read_resource;
				if ((l_end_string - l_start_string) == 0)
					goto ec;
				if (a_uri)
				{
					a_uri->host = (char *)malloc(l_end_string - l_start_string + 1);
					memcpy(a_uri->host, l_start_string, (l_end_string - l_start_string));
					a_uri->host[l_end_string - l_start_string] = '\0';
				}
				l_start_string = l_end_string;
				continue;
			}
		}
		else if (l_state == parse_state_read_port)
		{
			if (*l_end_string == '/')
			{
				l_state = parse_state_read_resource;
				/* check to make sure we're not going to overflow */
				if (l_end_string - l_start_string > 5)
					goto ec;
				/* check to make sure there was a port */
				if ((l_end_string - l_start_string) == 0)
					goto ec;
				/* copy the port into a temp buffer */
				memcpy(l_temp_port, l_start_string, l_end_string - l_start_string);
				/* convert it. */
				if (a_uri)
					a_uri->port = atoi(l_temp_port);
				l_start_string = l_end_string;
				continue;
			}
			else if (isdigit(*l_end_string) == 0)
			{
				/* check to make sure they are just digits */
				goto ec;
			}
		}
		/* next.. */
		l_end_string++;
		continue;
	}
  
  if (l_state == parse_state_read_host)
    {
      if ((l_end_string - l_start_string) == 0)
	goto ec;
      if (a_uri)
	{
	  a_uri->host = (char *)malloc(l_end_string - l_start_string + 1);
	  memcpy(a_uri->host, l_start_string, (l_end_string - l_start_string));
	  a_uri->host[l_end_string - l_start_string] = '\0';
	  /* for a "/" */
	  a_uri->resource = strdup("/");
	}
    }
  else if (l_state == parse_state_read_port)
    {
      if (strlen(l_start_string) == 0)
	/* oops.  that's not a valid number */
	goto ec;
      if (a_uri)
	{
	  a_uri->port = atoi(l_start_string);
	  a_uri->resource = strdup("/");
	}
    }
  else if (l_state == parse_state_read_resource)
    {
      if (strlen(l_start_string) == 0)
	{
	  if (a_uri)
	    a_uri->resource = strdup("/");
	}
      else
	{
	  if (a_uri)
	    a_uri->resource = strdup(l_start_string);
	}
    }
  else
    {
      /* uhh...how did we get here? */
      goto ec;
    }
  return 0;
	  
 ec:
  return -1;

#endif
}

http_uri *
http_uri_new(void)
{
  http_uri *l_return = NULL;

  l_return = (http_uri *)malloc(sizeof(http_uri));
  l_return->full = NULL;
  l_return->proto = NULL;
  l_return->host = NULL;
  l_return->port = 80;
  l_return->resource = NULL;
  return l_return;
}

void
http_uri_destroy(http_uri *a_uri)
{
  if (a_uri->full) {
    free(a_uri->full);
    a_uri->full = NULL;
  }
  if (a_uri->proto) {
    free(a_uri->proto);
    a_uri->proto = NULL;
  }
  if (a_uri->host) {
    free(a_uri->host);
    a_uri->host = NULL;
  }
  if (a_uri->resource) {
    free(a_uri->resource);
    a_uri->resource = NULL;
  }
  free(a_uri);
}

#if 0
#include <stdio.h>
void print_http_uri(char *str, http_uri *uri)
{
	if(str)
		printf("--- %s ---\n", str);
	
	printf("full: %s\n", uri->full);
	printf("proto: %s\n", uri->proto);
	printf("host: %s\n", uri->host);
	printf("port: %u\n", uri->port);
	printf("resource: %s\n", uri->resource);
}
void main()
{
	http_uri a_uri, a_uri_2;
	//char uri_s[1024] = "https://123.sogou.com/";
	//char uri_s[1024] = "http://192.168.0.1/new_index.htm";
	char uri_s[1024] = "https://www.baidu.com/s?tn=87048150_pg&word=strdup";
	http_uri_parse_2(uri_s, &a_uri_2);
	http_uri_parse(uri_s, &a_uri);

	print_http_uri("http_uri_parse_2", &a_uri_2);
	print_http_uri("http_uri_parse", &a_uri);
}
#endif


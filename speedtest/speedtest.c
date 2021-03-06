#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

#define ROUTER_360					0
#if ROUTER_360
#include "nc_ipc.h"
#define PRE_SET_SERVERLIST_XML		1
#define FOR_MULTI_PPPD				1
#if FOR_MULTI_PPPD
#include "nt_ipc.h"
#include "nt_kernel.h"
#include "igd/igd_ifstate.h"
#endif
#else
#include "libwf.h"
#endif

#define SPD_DEBUG_EN	1
#if SPD_DEBUG_EN
#if ROUTER_360
#define DEBUG(fmt, ...)	do{if(1)console_printf("\nSPD[%s-%d] "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#else
#define DEBUG(fmt, ...)	do{if(1)printf("\n[%s-%d] "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#endif
#else
#define DEBUG(fmt, ...)	do{}while(0)
#endif

#ifdef ENABLE_SPD_LOG
#define spdlog(fmt, args...)    spdlog_printf(fmt, ##args)
#else
#define spdlog(fmt, args...)	printf(fmt, ##args)
#endif
#define SPD_DEBUG_LOG   "/tmp/spd_log"

int spdlog_printf(const char *fmt, ...)
{
        FILE *f = NULL;
        va_list ap;

        f = fopen(SPD_DEBUG_LOG, "a+");
        if (f == NULL) {
                return -1;
        }

        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fclose(f);

        return 0;
}

#if ROUTER_360
void print_strn(char *str, unsigned int max_num)
{
	printf("[print_strn]:");
	while(max_num && str && (*str != '\0'))
	{
		printf("%c", *str);
		++str;
		--max_num;
	}
	printf("\n");
}
#endif

struct speed_config
{
	double client_lat;
	double client_lon;
	char client_isp[64];
};

struct speed_server
{
	struct list_head list;
	double distance;
	double lat;
	double lon;
	unsigned int ip;		// use for get_latency
	char *host;			// use for get_latency
	char url[256];
	char cut_url[256];
	char country[32];
	char sponsor[64];
};

struct speed_server_list
{
	struct list_head server_head;
	unsigned int server_num;
	unsigned int max_num;
};

#define DOWN_PROCESS_NUM	5	// <= 10
#define DOWN_TIME			18
#define UP_PROCESS_NUM		5	// <= 10
#define UPLOAD_TIME			15
struct speed_child
{
	pid_t download_pid[DOWN_PROCESS_NUM];
	pid_t upload_pid[UP_PROCESS_NUM];
	char url[DOWN_PROCESS_NUM][256];
	char upload_url[UP_PROCESS_NUM][256];
	char download_if[DOWN_PROCESS_NUM][12];
};

#define SPEEDTEST_CONFIG_URL		"http://www.speedtest.net/speedtest-config.php"
#define SPEEDTEST_SERVERLIST_URL1	"http://www.speedtest.net/speedtest-servers-static.php"
#define SPEEDTEST_SERVERLIST_URL2	"http://c.speedtest.net/speedtest-servers-static.php"
#define SPEEDTEST_SERVERLIST_URL3	"http://www.speedtest.net/speedtest-servers.php"
#define SPEEDTEST_SERVERLIST_URL4	"http://c.speedtest.net/speedtest-servers.php"
#define CLOSEST_SERVER_NUM_MAX	3
#define SPEEDTEST_CONFIG_XML				"/tmp/speedtest_config.xml"
#if PRE_SET_SERVERLIST_XML
#define SPEEDTEST_SERVER_LIST_XML			"/sh/speedtest_server_list.xml"
#else
#define SPEEDTEST_SERVER_LIST_XML			"/tmp/speedtest_server_list.xml"
#endif
#define SPEEDTEST_CLOSEST_SERVERS_XML	"/tmp/speedtest_closest_servers.xml"
//#define SPEEDTEST_UPLOAD_XML				"/tmp/speedtest_upload.xml"

#if ROUTER_360
static int waitpid_sec(pid_t pid, int *pstatus, unsigned int max_time)
{
	unsigned int time_count = 0;

	if(max_time){
		while(1){
			if(time_count >= max_time)
				return time_count;
			if(waitpid(pid, pstatus, WNOHANG) > 0)
				return time_count;
			sleep(1);
			++time_count;
		}
	}
	else
		return waitpid(pid, pstatus, 0);
}

static int create_child_process(const char *exename, char *const argv[], int close_std, int *pipefd)
{
	pid_t pid;
	int fd;

	pid = fork();
	if(pid < 0)
		return pid;
	else if(pid > 0)
		return pid;

	if(pipefd){
		dup2(pipefd[1],STDOUT_FILENO);
		dup2(pipefd[1],STDERR_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
	}

	if(close_std && !pipefd){
		fd = open("/dev/null", O_WRONLY);
		if(fd >= 0){
			if(fd != STDOUT_FILENO)
				dup2(fd, STDOUT_FILENO);
			if(fd != STDERR_FILENO)
				dup2(fd, STDERR_FILENO);
			close(fd);
		}
	}
	
	execvp(exename, argv);
	exit(1);
}
#endif

enum{
	WGET_CONFIG,
	WGET_SERVER_LIST1,
	WGET_SERVER_LIST2,
	WGET_SERVER_LIST3,
	WGET_SERVER_LIST4,
	WGET_DOWNLOAD,
	WGET_POST_UPLOAD
};
#if ROUTER_360
#define WGET_PATH	"/bin/wget"
#else
#define WGET_PATH	"/usr/bin/wget"
#endif

int speedtest_child(int option, void *data, char *if_name)
{
	int ret = -1, i=-1;
	char *argv[10];

	switch(option)
	{
	case WGET_CONFIG:
		argv[++i] = "wget";
		argv[++i] = "-O";
		argv[++i] = SPEEDTEST_CONFIG_XML;
		argv[++i] = SPEEDTEST_CONFIG_URL;
		argv[++i] = NULL;
		ret = create_child_process(WGET_PATH, argv, 0, NULL);
		break;
	case WGET_SERVER_LIST1:
	case WGET_SERVER_LIST2:
	case WGET_SERVER_LIST3:
	case WGET_SERVER_LIST4:
		argv[++i] = "wget";
		argv[++i] = "-O";
		argv[++i] = SPEEDTEST_SERVER_LIST_XML;
		if(option == WGET_SERVER_LIST1)
			argv[++i] = SPEEDTEST_SERVERLIST_URL1;
		else if(option == WGET_SERVER_LIST2)
			argv[++i] = SPEEDTEST_SERVERLIST_URL2;
		else if(option == WGET_SERVER_LIST3)
			argv[++i] = SPEEDTEST_SERVERLIST_URL3;
		else if(option == WGET_SERVER_LIST4)
			argv[++i] = SPEEDTEST_SERVERLIST_URL4;
		argv[++i] = NULL;
		ret = create_child_process(WGET_PATH, argv, 0, NULL);
		break;
	case WGET_DOWNLOAD:
		argv[++i] = "wget";
		argv[++i] = "-O";
		argv[++i] = "/dev/null";
		if(if_name && (strlen(if_name) > 0)){
			argv[++i] = "-i";
			argv[++i] = if_name;
		}
		argv[++i] = (char *)data;
		argv[++i] = NULL;
		ret = create_child_process(WGET_PATH, argv, 1, NULL);
		break;
	case WGET_POST_UPLOAD:
		argv[++i] = "wget";
		argv[++i] = "--post-file=/bin/busybox";
		argv[++i] = "-O";
		argv[++i] = "/dev/null";
		argv[++i] = (char *)data;
		argv[++i] = NULL;
		ret = create_child_process(WGET_PATH, argv, 1, NULL);
		break;
	default:
		break;
	}
	return ret;
}

static unsigned char speed_chunk[1228800];		// 1200 KB
static struct speed_config g_config;
static struct speed_server_list closest_server_list;
static struct speed_child speed_child_info;

void init_server_list(struct speed_server_list *list, unsigned int max_size)
{
	INIT_LIST_HEAD(&list->server_head);
	list->server_num = 0;
	list->max_num = max_size;
}

struct speed_server *insert_server_list(struct speed_server_list *list, struct speed_server *server)
{
	struct speed_server *pos, *ret=NULL; 
	
	if(list_empty_careful(&list->server_head))
		list_add(&server->list, &list->server_head);
	else{
		list_for_each_entry(pos, &list->server_head, list){
			if(server->distance < pos->distance){ 
				list_add_tail(&server->list, &pos->list);
				
				if(list->server_num >= list->max_num){
					ret = list_entry(list->server_head.prev, struct speed_server, list);
					list_del(list->server_head.prev);
				}
				goto END;
			}
		}
		
		if(list->server_num >= list->max_num){
			ret = server;
			goto END;
		}
		else
			list_add_tail(&server->list, &list->server_head); 
	}

END:
	if(!ret)
		++list->server_num;
	return ret;
}

void free_server_list(struct speed_server_list *list)
{
	struct speed_server *pos, *n;

	if(list_empty_careful(&list->server_head))
		return;
	list_for_each_entry_safe(pos, n, &list->server_head, list)
		free(pos);
	INIT_LIST_HEAD(&list->server_head);
}

void print_server_list(struct speed_server_list *list)
{
	struct speed_server *pos;

	spdlog("---list size: %u \n", list->server_num);
	if(list_empty_careful(&list->server_head))
		return;
	list_for_each_entry(pos, &list->server_head, list){
		spdlog("[distance: %lf] url=%s lat=%lf lon=%lf country=%s sponsor=%s \n\n", 
			pos->distance, pos->url, pos->lat, pos->lon, pos->country, pos->sponsor);
	}
}

static char *get_common_headers()
{
	static char header[256] = "User-Agent: Mozilla/5.0 (Linux; U; 32bit; en-us) Python/2.7.3 (KHTML, like Gecko) speedtest-cli/0.3.4\r\n"
							"Accept-Encoding: identity\r\n"
							"Connection: close\r\n";
	return &header[0];
}

static int cp_file(char *src, char *dst)
{
	FILE *fp = NULL, *fp_tmp = NULL;
	char buf[1024] = {0};

	fp_tmp = NULL;
	fp = NULL;

	fp = fopen(src, "r");
	if(!fp){
		DEBUG("open %s failed\n",src);
		return -1;
	}

	fp_tmp = fopen(dst, "w+");
	if(!fp_tmp){
		DEBUG("open %s failed\n",dst);
		fclose(fp);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	fseek(fp_tmp, 0, SEEK_SET);

	while(!feof(fp)){
		memset(buf, 0, sizeof(buf));
		fgets(buf, sizeof(buf), fp);
		fputs(buf, fp_tmp);
	}

	fclose(fp);
	fclose(fp_tmp);

	return 0;
}

char *get_string_value(char *key, char *in, char *out, unsigned int out_size)
{
	unsigned int count = 0;
	char *ptr, *pout = out, *tmp;
	
	if(!key || !in || !out || !out_size)
		return NULL;

	tmp = strstr(in, key);
	if(!tmp)
		return NULL;
	ptr = tmp + strlen(key) + 2;

	while(ptr && (*ptr != '\0'))
	{
		if(*ptr == '\"'){
			break;
		}
		
		*pout = *ptr;
		++pout; ++ptr;
		++count;
		if(count >= (out_size-1))
			break;
	}

	if(!count)
		return NULL;
	*pout = '\0';
	
	return ++ptr;
}

char *get_double_value(char *key, char *in, double *out)
{
	double value = 0.0;
	char *ptr, *tmp;
	
	if(!key || !in || !out)
		return NULL;

	tmp = strstr(in, key);
	if(!tmp)
		return NULL;
	ptr = tmp + strlen(key) + 2;
	value = atof(ptr);
	*out = value;

	while(ptr && (*ptr != '\0'))
	{
		if(*ptr == '\"'){
			break;
		}
		++ptr;
	}

	return ++ptr;
}

char *get_client_xml(char *in, struct speed_config *config)
{
	char *ptr, *tmp;

	ptr = strstr(in, "<client");
	if(!ptr){
		DEBUG("don't find <client> in config-xml \n");
		return NULL;
	}

	tmp = get_double_value("lat", ptr+7, &config->client_lat);
	if(!tmp){
		DEBUG("don't find <lat> in <client> \n");
		return NULL;
	}
	ptr = tmp;

	tmp = get_double_value("lon", ptr, &config->client_lon);
	if(!tmp){
		DEBUG("don't find <lon> in <client> \n");
		return NULL;
	}
	ptr = tmp;

	tmp = get_string_value("isp", ptr, config->client_isp, sizeof(config->client_isp)-1);
	if(!tmp){
		DEBUG("don't find <isp> in <client> \n");
		return ptr;
	}
	
	return tmp;
}

int getconfig(struct speed_config *config)
{
	int ret = 0;
	FILE *fp;

	if(!config)
		return -1;
	if(access(SPEEDTEST_CONFIG_XML, F_OK) == 0)
		goto XML_EXIST;
	
	ret = speedtest_child(WGET_CONFIG, NULL, NULL);
	if(ret <= 0){
		DEBUG("get "SPEEDTEST_CONFIG_XML" failed \n");
		return -1;
	}
	waitpid_sec(ret, NULL, 0);

	if(access(SPEEDTEST_CONFIG_XML, F_OK)){
		DEBUG("get "SPEEDTEST_CONFIG_XML" failed \n");
		return -1;
	}
XML_EXIST:
	DEBUG("get "SPEEDTEST_CONFIG_XML" OK \n");
	fp = fopen(SPEEDTEST_CONFIG_XML, "r");
	if(!fp){
		DEBUG("open "SPEEDTEST_CONFIG_XML" failed \n");
		return -1;
	}

	memset(speed_chunk, 0, sizeof(speed_chunk));
	if(fread(speed_chunk, 1, sizeof(speed_chunk), fp) <= 0){
		DEBUG("read "SPEEDTEST_CONFIG_XML" failed \n");
		fclose(fp);
		return -1;
	}
	fclose(fp);

	if(!get_client_xml((char *)speed_chunk, config))
		return -1;
	
	return 0;
}

char *get_server_xml(char *in, struct speed_server *server)
{
	char *ptr = in, *tmp;
	
	if(!in || !server)
		return NULL;

	ptr = strstr(in, "<server");
	if(!ptr){
		DEBUG("don't find <server> in server-list-xml \n");
		//print_strn(in, 1024);
		return NULL;
	}
	tmp = get_string_value("url", ptr+7, server->url, sizeof(server->url)-1);
	if(!tmp){
		DEBUG("don't find <url> in <server> \n");
		return NULL;
	}
	ptr = tmp;
	strcpy(server->cut_url, server->url);
	tmp = strstr(server->cut_url, "/speedtest/");
	if(tmp)
		*tmp = '\0';

	tmp = get_double_value("lat", ptr, &server->lat);
	if(!tmp){
		DEBUG("don't find <lat> in <server> \n");
		return NULL;
	}
	ptr = tmp;

	tmp = get_double_value("lon", ptr, &server->lon);
	if(!tmp){
		DEBUG("don't find <lon> in <server> \n");
		return NULL;
	}
	ptr = tmp;

	tmp = get_string_value("country", ptr, server->country, sizeof(server->country)-1);
	if(!tmp){
		DEBUG("don't find <country> in <server> \n");
		return NULL;
	}
	ptr = tmp;

	tmp = get_string_value("sponsor", ptr, server->sponsor, sizeof(server->sponsor)-1);
	if(!tmp){
		DEBUG("don't find <sponsor> in <server> \n");
		return NULL;
	}

	return tmp;
}

int isfar(double lat1, double lon1, double lat2, double lon2)
{
	double dlat, dlon;

	dlat = lat2 - lat1;
	//DEBUG("lat1: %lf  lon1: %lf  lat2: %lf  lon2: %lf    dlat: %lf  dlon: %lf \n", lat1, lon1, lat2, lon2, dlat, lon2-lon1);
	if(dlat > 8.0 || dlat < -8.0)
		return 1;
	dlon = lon2 - lon1;
	if(dlon > 8.0 || dlon < -8.0)
		return 1;
	return 0;
}

#if ROUTER_360
#ifndef M_PI
#define M_PI            3.14159265358979323846  /* pi */
#endif
double radian(double angle)
{
	return ((M_PI/180.0) * angle);
}
#endif

double distance(double lat1, double lon1, double lat2, double lon2)
{
	double radius = 6371.0;  // km
	double dlat = radian(lat2 - lat1);
	double dlon = radian(lon2 - lon1);
	double a = 0.0, c = 0.0, d = 0.0;

	a = (sin(dlat / 2.0) * sin(dlat / 2.0) +         
			cos(radian(lat1)) * cos(radian(lat2)) * sin(dlon / 2.0) * sin(dlon / 2.0));
	c = 2.0 * atan2(sqrt(a), sqrt(1 - a));
	d = radius * c;

	return d;
}

int peek_closest_server(struct speed_config *config, struct speed_server_list *list)
{
	struct speed_server server_tmp;
	char *ptr = (char *)(&speed_chunk[0]), *tmp;
	int ret = 0, count = 0, near_count = 0;

	struct speed_server *insert = NULL, *del = NULL;
	
	if(!config || !list)
		return -1;

	//print_strn(ptr, 1024);
	while(ptr && (*ptr != '\0'))
	{
		tmp = get_server_xml(ptr, &server_tmp);
		if(!tmp)
			break;
		ptr = tmp;
		++count;
		if( isfar(config->client_lat, config->client_lon, server_tmp.lat, server_tmp.lon) )
			continue;
		++near_count;
		server_tmp.distance = distance(config->client_lat, config->client_lon, server_tmp.lat, server_tmp.lon);
		DEBUG("distance: %lf  [%d - %d]\n", server_tmp.distance, count, near_count);
		
		insert = (struct speed_server *)malloc(sizeof(struct speed_server));
		if(!insert){
			DEBUG("malloc failed \n");
			continue;
		}
		memcpy(insert, &server_tmp, sizeof(struct speed_server));
		insert->ip = 0;
		insert->host = NULL;
		INIT_LIST_HEAD(&insert->list);
		del = insert_server_list(list, insert);
		if(del)
			free(del);
	}
	DEBUG("count: %d   near_count: %d \n", count, near_count);

	return ret;
}

int get_server_lists(struct speed_config *config, struct speed_server_list *list)
{
	int i = 0, ret = 0, parse_xml = 0;
	size_t r_read;
	FILE *fp;

	if(!config || !list)
		return -1;
	if(access(SPEEDTEST_SERVER_LIST_XML, F_OK) == 0)
		goto XML_EXIST;

	for(i=WGET_SERVER_LIST1; i<=WGET_SERVER_LIST4; i++){
		ret = speedtest_child(i, NULL, NULL);
		if(ret <= 0){
			DEBUG("get "SPEEDTEST_SERVER_LIST_XML" failed \n");
			goto NEXT;
		}
		waitpid_sec(ret, NULL, 0);

		if(access(SPEEDTEST_SERVER_LIST_XML, F_OK)){
			DEBUG("get "SPEEDTEST_SERVER_LIST_XML" failed \n");
			goto NEXT;
		}
XML_EXIST:
		DEBUG("get "SPEEDTEST_SERVER_LIST_XML" OK \n");
		fp = fopen(SPEEDTEST_SERVER_LIST_XML, "r");
		if(!fp){
			DEBUG("open "SPEEDTEST_SERVER_LIST_XML" failed \n");
			return -1;
		}

		memset(speed_chunk, 0, sizeof(speed_chunk));
		r_read = fread(speed_chunk, 1, sizeof(speed_chunk), fp);
		if(r_read <= 0){
			DEBUG("read "SPEEDTEST_SERVER_LIST_XML" failed \n");
			fclose(fp);
			return -1;
		}
		DEBUG("r_read=%d \n", r_read);
		fclose(fp);
		parse_xml = 1;
		break;
		
	NEXT:
		DEBUG("goto next, cur: %d \n", i);
	}

	if(!parse_xml)
		return -1;

	ret = peek_closest_server(config, list);
	if(ret < 0){
		DEBUG("peek closest servers failed: %d \n", ret);
		return ret;
	}

	return 0;
}

int save_closest_servers_file(struct speed_server_list *list)
{
	struct speed_server *pos;
	FILE *fp;
	
	if(!list)
		return -1;
	if(list_empty_careful(&list->server_head))
		return -1;

	fp = fopen(SPEEDTEST_CLOSEST_SERVERS_XML, "w");
	if(!fp){
		DEBUG("open "SPEEDTEST_CLOSEST_SERVERS_XML" failed \n");
		return -1;
	}
		
	list_for_each_entry(pos, &list->server_head, list){
		fprintf(fp, "<server url=\"%s\" lat=\"%lf\" lon=\"%lf\" country=\"%s\" sponsor=\"%s\" distance=\"%lf\" />\n", 
			pos->url, pos->lat, pos->lon, pos->country, pos->sponsor, pos->distance);
	}
	fclose(fp);
	return 0;
}

char *get_closest_server_xml(char *in, struct speed_server *server)
{
	char *ptr, *tmp;

	tmp = get_server_xml(in, server);
	if(!tmp){
		return NULL;
	}
	ptr = tmp;

	tmp = get_double_value("distance", ptr, &server->distance);
	if(!tmp){
		DEBUG("don't find <distance> in <server> \n");
		return ptr;
	}

	return tmp;
}

int read_closest_servers_file(struct speed_server_list *list)
{
	FILE *fp;
	char *ptr, *tmp;
	size_t r_read;
	struct speed_server *server_tmp = NULL, *del = NULL;
	
	fp = fopen(SPEEDTEST_CLOSEST_SERVERS_XML, "r");
	if(!fp){
		DEBUG("open "SPEEDTEST_CLOSEST_SERVERS_XML" failed \n");
		return -1;
	}

	memset(speed_chunk, 0, sizeof(speed_chunk));
	r_read = fread(speed_chunk, 1, sizeof(speed_chunk), fp);
	if(r_read <= 0){
		DEBUG("read "SPEEDTEST_CLOSEST_SERVERS_XML" failed \n");
		fclose(fp);
		return -1;
	}
	fclose(fp);

	ptr = (char *)(&speed_chunk[0]);
	while(ptr && (*ptr != '\0'))
	{
		server_tmp = (struct speed_server *)malloc(sizeof(struct speed_server));
		if(!server_tmp){
			DEBUG("malloc failed \n");
			continue;
		}
		tmp = get_closest_server_xml(ptr, server_tmp);
		if(!tmp){
			free(server_tmp);
			break;
		}
		ptr = tmp;

		server_tmp->ip = 0;
		server_tmp->host = NULL;
		INIT_LIST_HEAD(&server_tmp->list);
		del = insert_server_list(list, server_tmp);
		if(del)
			free(del);
	}

	if(list_empty_careful(&list->server_head)){
		return -1;
	}
	return 0;
}

#if ROUTER_360
static int wf_gethostbyname(char *name, char *ip, unsigned int *addr)
{
	struct hostent *host = NULL;
	struct in_addr host_addr;
	char *ptr = name;

	if(strncmp(ptr, "http://", 7) == 0)
		ptr += 7;
	else if(strncmp(ptr, "https://", 8) == 0)
		ptr += 8;

	host = gethostbyname(ptr);
	if(!host)
		return -1;
	host_addr = *((struct in_addr *)(host->h_addr));
	//memcpy(&host_addr.s_addr, host->h_addr_list[0], sizeof(unsigned int));
	if(ip){
		inet_pton(host->h_addrtype, host->h_addr, ip);
		//sprintf(ip, "%s",(char *)inet_ntoa(host_addr));
	}
	if(addr)
		*addr = host_addr.s_addr;
	return 0;
}
static int wf_connect_addr(int clientSock, unsigned int serverAddr, int serverPort)
{
	struct hostent *host = NULL;
	struct sockaddr_in addr;
	
	if( clientSock < 0 || serverAddr == 0 || serverPort <= 0 )
		return -1;
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = serverAddr;
	addr.sin_family =AF_INET;
	addr.sin_port = htons(serverPort);

	return connect(clientSock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
}
static int wf_send(int sock, unsigned char *buf, int total_len, int flag)
{
	int len=0, i=0, next=total_len;
	
	while(next > 0 && (len=send(sock, buf+i, next, flag)) != next)
	{
		if(len>0)
		{
			i += len;
			next -= len;
		}
		else if(len == 0)
			break;
		else if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			continue;
		else
			return -1;
	}
	if(!i)
		return len;

	return i;
}
static int wf_recv(int sock, unsigned char *buf, int total_len, int flag)
{
	int len=0, i=0, next=total_len;
	
	while(next > 0 && (len=recv(sock, buf+i, next, flag)) != next)
	{
		if(len>0)
		{
			i += len;
			next -= len;
		}
		else if(len == 0)
			break;
		else if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			continue;
		else
			return -1;
	}

	return i;
}
#endif

static char latency_head[1024] = "GET /speedtest/latency.txt HTTP/1.1\r\n"
							"User-Agent: Mozilla/5.0 (Linux; U; 32bit; en-us) Python/2.7.3 (KHTML, like Gecko)speedtest-cli/0.3.4\r\n"
							"Accept-Encoding: identity\r\n"
							"Connection: close\r\n"
							"Host: ";
static int send_latency_request(int sock_fd, char *url)
{
	char data[1024];
	int data_len = 0;

	data_len = sprintf(data, "%s%s\r\n\r\n", latency_head, url);
	return wf_send(sock_fd, (unsigned char *)data, data_len, 0);
}
int get_latency(struct speed_server *server, unsigned long *time_ms)
{
	int ret = 0, sock_fd = 0;
	fd_set readable;
	struct timeval wait_tv, time_delay, tv1, tv2;
	int max_fd;
	unsigned char data[1024];

	sock_fd = socket(AF_INET,SOCK_STREAM,0);
	if(sock_fd < 0)
		return -1;

	if(server->ip == 0){
		if(server->host == NULL){
				if(strncmp(server->cut_url, "http://", 7) == 0)
					server->host = server->cut_url + 7;
				else if(strncmp(server->cut_url, "https://", 8) == 0)
					server->host = server->cut_url + 8;
				else
					server->host = server->cut_url;
		}
		//DEBUG("gethostbyname: %s \n", server->host);
		ret = wf_gethostbyname(server->host, NULL, &server->ip);
		if(ret < 0){
			DEBUG("gethostbyname error: %s \n", server->host);
			goto ERR;
		}
	}
	
	ret = wf_connect_addr(sock_fd, server->ip, 8080);
	if(ret < 0){
		DEBUG("connect error: %s \n", server->cut_url);
		goto ERR;
	}
	gettimeofday(&tv1,NULL);
	ret = send_latency_request(sock_fd, server->host);
	if(ret < 0)
		goto ERR;

	while(1)
	{
		FD_ZERO(&readable);
		FD_SET(sock_fd, &readable);
		max_fd = sock_fd + 1;
		wait_tv.tv_sec = 0;
		wait_tv.tv_usec = 100000;		// 100 ms = 100 * 1000

		ret = select(max_fd, &readable, NULL, NULL, &wait_tv);
		if (ret < 0){
			if (errno == EINTR){
				continue;
			}
			DEBUG("TIME_OUT     1\n");
			goto TIME_OUT;
		}
		else if (ret == 0){
			DEBUG("TIME_OUT     2\n");
			goto TIME_OUT;
		}
		else{
			if (FD_ISSET(sock_fd, &readable)){
				if( wf_recv(sock_fd, data, sizeof(data), 0) < 0){
					DEBUG("TIME_OUT     3\n");
					goto TIME_OUT;
				}
				gettimeofday(&tv2,NULL);
			}
		}
		break;
	}
	close(sock_fd);
	*time_ms = (unsigned long)(((tv2.tv_sec - tv1.tv_sec) * 1000) + ((tv2.tv_usec - tv1.tv_usec) / 1000));
	return 0;

ERR:
	close(sock_fd);
	*time_ms = 100;
	DEBUG("get latency.txt failed: %s/speedtest/latency.txt \n", server->cut_url);
	return -1;
TIME_OUT:
	close(sock_fd);
	*time_ms = 100;
	return 0;
}

int select_best_server(struct speed_server_list *list, struct speed_server **best)
{
	struct speed_server *pos;
	unsigned long time_avg = 0, time_min = 0xFFFFFFFF, time_tmp = 0;
	int i=0, j=0;
	
	if(!list || !best)
		return -1;
	if(list_empty_careful(&list->server_head))
		return -1;

	list_for_each_entry(pos, &list->server_head, list){
		time_avg = 0;
		for(i=0; i<6; i++){
			time_tmp = 1000;
			get_latency(pos, &time_tmp);
			time_avg += time_tmp;
		}
		time_avg = time_avg / 6;
		DEBUG("time_avg=%lu \n", time_avg);
		if(time_avg < time_min){
			*best = pos;
			time_min = time_avg;
		}
		++j;
	}
	return 0;
}

#if ROUTER_360
/*
typedef struct _Recv_property_t{
    unsigned long long rbytes;
    unsigned int packets;
    int errs;
    int drop;
    int fifo;
    int frame;
    int compressed;
    int multicast;
}Recv_pro,*pRecv_pro;
typedef struct _Transmit_property_t{
    unsigned long long sbytes;
    unsigned int packets;
    int errs;
    int drop;
    int fifo;
    int colls;
    int carrier;
    int compressed;
}Transmit_pro,*pTransmit_pro;
*/
typedef struct _Time_jiffes_t{
    long jiffes_sec;
    long jiffes_usec;
}Time_jiffes,*pTime_jiffes;
/*
typedef struct _devInterface_t{
    char name[32];
    Recv_pro Rpacket;
    Transmit_pro Spacket;
    Time_jiffes jiffes;
}devInterface,*pdevInterface;
*/
typedef struct _statistics_data_t{
    unsigned long long packetsize;
    Time_jiffes jiffes;
}Statistic_data,*pStatistic_data;
enum{
    START=0,
    RBYTES,
    RPACKETS,
    RERRS,
    RDROP,
    RFIFO,
    RFRAME,
    RCOMPRESSED,
    RMULTICAST,
    SBYTES,
    SPACKETS,
    SERRS,
    SDROP,
    SFIFO,
    SCOLLS,
    SCARRIER,
    SCOMPRESSED,
    TJIFFESSEC,
    TJIFFESUSEC
};

#ifdef PLATFORM_MTK
#define WIRED_INTERFACE "eth2.1"
#define WISP_INTERFACE "apcli0"
#define WISP_INTERFACE_5G "apclii0"
#else//rtk
#define WIRED_INTERFACE "eth1.1"
#define WISP_INTERFACE		"wlan0-vxd"
#endif

char *ispacing(char *str)
{
	while(*str==' ')
		str++;
	return str ;
}
int isnspacing(char *str)
{
	int len = 0;	
	while(*str!=' ' && *str!='\0' && *str!='\n'){
		str++;
		len++;
	}
	return len;
}
int getdata(char *dest, char *data, int line)
{
	char *node=NULL,*tmpstr=NULL;
	int len=0;
	
	while(line)
	{
		node = ispacing(data);
		tmpstr=node;
		len = isnspacing(tmpstr);
		--line;
		if(!line){
			strncpy(dest,node,len);
			return 0;
		}
		data=node;
		data+=len;
		len = 0;
	}
	return -1;
}
int getRxdata(char *dest, char *data, int line)
{
	int ret=0;
	
	ret = getdata(dest,data,line);
	if(ret){
		DEBUG("get rx data error!\n");
		return -1;
	}
	return 0;
}
static int readInterfaceData(Statistic_data *cout, char *facetype, int recv)
{
	FILE *fp=NULL;
	char linebuf[256]={0};
	char destbuf[18]={0};
	char *pod=NULL;
	
	fp = fopen("/proc/net/dev","r");
	while(fgets(linebuf, sizeof(linebuf)-1, fp) !=NULL)
	{
		pod = strstr(linebuf,facetype);
		if(!pod)
			continue;
		pod+=strlen(facetype)+1;
		if(recv)
			getRxdata(destbuf,pod, RBYTES);
		else
			getRxdata(destbuf,pod, SBYTES);
		cout->packetsize=atoll(destbuf);
		pod = NULL;
		
		memset(destbuf,0,sizeof(destbuf));
		pod = strstr(linebuf,facetype);
		pod+=strlen(facetype)+1;
		getRxdata(destbuf,pod, TJIFFESSEC);
		cout->jiffes.jiffes_sec=atol(destbuf);
		pod = NULL;
		
		memset(destbuf,0,sizeof(destbuf));
		pod = strstr(linebuf,facetype);
		pod+=strlen(facetype)+1;
		getRxdata(destbuf,pod, TJIFFESUSEC);
		cout->jiffes.jiffes_usec=atol(destbuf);
	}
	fclose(fp);
	return 0;
}
static int get_netdev_flowNew(Statistic_data *cout, int recv)
{
	Statistic_data tmp_bytes;
	memset(&tmp_bytes,0,sizeof(tmp_bytes));
    
	if(readInterfaceData(&tmp_bytes,WIRED_INTERFACE, recv)>=0)
		cout->packetsize += tmp_bytes.packetsize;
	cout->jiffes.jiffes_sec = tmp_bytes.jiffes.jiffes_sec;
	cout->jiffes.jiffes_usec = tmp_bytes.jiffes.jiffes_usec;
	spdlog("%-12s  %-12llu  %ld  %ld \n", WIRED_INTERFACE, tmp_bytes.packetsize, tmp_bytes.jiffes.jiffes_sec, tmp_bytes.jiffes.jiffes_usec);

	memset(&tmp_bytes,0,sizeof(tmp_bytes));

	if(readInterfaceData(&tmp_bytes,WISP_INTERFACE, recv) >= 0)
		cout->packetsize+=tmp_bytes.packetsize;
	spdlog("%-12s  %-12llu  %ld  %ld \n", WISP_INTERFACE, tmp_bytes.packetsize, tmp_bytes.jiffes.jiffes_sec, tmp_bytes.jiffes.jiffes_usec);

#ifdef PLATFORM_MTK
	if(readInterfaceData(&tmp_bytes,WISP_INTERFACE_5G, recv) >= 0)
		cout->packetsize+=tmp_bytes.packetsize;
	spdlog("%-12s  %-12llu  %ld  %ld \n", WISP_INTERFACE_5G, tmp_bytes.packetsize, tmp_bytes.jiffes.jiffes_sec, tmp_bytes.jiffes.jiffes_usec);
#endif
	
	return 0;
}

enum {
    SPD_STATUS_INIT=0,
    SPD_STATUS_RUN,
    SPD_STATUS_END,
};
#define SPEEDTEST_RESULT	"/tmp/test_speed"

static int save_speed_result(const char *fmt, ...)
{
	int fd;
	char s[1024] = {0};
	va_list ap;

	va_start(ap,fmt);
	vsprintf(s,fmt,ap);
	va_end(ap);
	
	fd = open(SPEEDTEST_RESULT, O_WRONLY|O_CREAT|O_TRUNC);
	write(fd,s,strlen(s));
	close(fd);
	return 0;
}

int change_speed_result_status()
{
	FILE* fp = NULL;
	char buf[1024] = {0}, buf_tmp[1024] = {0};;
	char status[10] = "\"status\":";
	char *p = NULL;

	if(access(SPEEDTEST_RESULT, F_OK))
		return -1;

	fp = fopen(SPEEDTEST_RESULT, "r");
	if(!fp){
		DEBUG("open "SPEEDTEST_RESULT" failed\n");
		fclose(fp);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	fgets(buf, sizeof(buf), fp);
	p = strstr(buf, status);
	if(!p){
		DEBUG("can not find status\n");
		fclose(fp);
		return -1;
	}

	strncpy(buf_tmp, buf + sizeof(status), sizeof(buf_tmp));
	fclose(fp);
	save_speed_result("\"status\":%d%s", SPD_STATUS_END, buf_tmp);
	return 0;
}

Statistic_data down_count[DOWN_TIME];
Statistic_data down_diff_count[DOWN_TIME];
Statistic_data up_count[UPLOAD_TIME];
Statistic_data up_diff_count[UPLOAD_TIME];
static uint64_t down_speed = 0, down_max_speed = 0;
static uint64_t up_speed = 0, up_max_speed = 0;

long get_diff_of_count(Statistic_data *count, Statistic_data *diff_count, int time_count)
{
	long diff = 0;

	if (count[time_count-1].packetsize>0 && count[time_count].packetsize>=count[time_count-1].packetsize) {
		diff = count[time_count].packetsize - count[time_count-1].packetsize;
	}
	diff_count[time_count-1].packetsize = diff;
	
	if(count[time_count].jiffes.jiffes_sec > count[time_count-1].jiffes.jiffes_sec+1){
		diff_count[time_count-1].jiffes.jiffes_sec = count[time_count].jiffes.jiffes_sec-count[time_count-1].jiffes.jiffes_sec;
	}
	else {
		diff_count[time_count-1].jiffes.jiffes_sec=1;
	}
	
	if(count[time_count].jiffes.jiffes_usec > count[time_count-1].jiffes.jiffes_usec){
		diff_count[time_count-1].jiffes.jiffes_usec = count[time_count].jiffes.jiffes_usec-count[time_count-1].jiffes.jiffes_usec;
	}

	return diff;
}

void swapStatistic(Statistic_data *p1,Statistic_data *p2) { 
	Statistic_data p; 
	memcpy(&p,p1,sizeof(Statistic_data)); 
	memcpy(p1,p2,sizeof(Statistic_data)); 
	memcpy(p2,&p,sizeof(Statistic_data)); 
}

static uint64_t calc_speed(Statistic_data *count, Statistic_data *diff_count, int time_count, uint64_t *max_speed)
{
	int start, end;
	int i,k;
	float max_value=0, min_value=0;  
	float tmptime=0.0,total_value=0.0,sub_value=0.0;

	//start = COUNT_NUM>=3?2:0;
	start = 0;
	spdlog("------ all diff_count -------\n");
	for (i=start; i<time_count-1; i++) {		
		if (diff_count[i].packetsize <= 0) 
			continue;
		spdlog("%d \t%7llu \t%ld \t%ld \n", i, diff_count[i].packetsize, diff_count[i].jiffes.jiffes_sec, diff_count[i].jiffes.jiffes_usec);
		tmptime=(float)diff_count[i].jiffes.jiffes_sec+(float)diff_count[i].jiffes.jiffes_usec/(1000000.00);

		if ( diff_count[i].packetsize/tmptime < min_value || min_value == 0 )
			min_value = diff_count[i].packetsize/tmptime;
		if ( diff_count[i].packetsize/tmptime > max_value || min_value == 0 )
			max_value = diff_count[i].packetsize/tmptime;
	}
	if(max_speed)
		*max_speed = (uint64_t)max_value;
	//if(min_speed)
		//*min_speed = (uint64_t)min_value;

	// the total number of data: COUNT_NUM-1
	// when COUNT_NUM>=3, discard the first and second data
	// bubble sort:  start index ~ end index
	start = time_count>=3?2:0;
	end = time_count - 2;
	for(i=start+1; i<end; i++){
		for(k=start; k<end+1+start-i; k++){
			if(diff_count[k].packetsize > diff_count[k+1].packetsize){
				swapStatistic(&diff_count[k], &diff_count[k+1]);
			}
		}
	}

	// take the largest four, then discard the biggest
	spdlog("------ 3 diff_count -------\n");
	for(i=end-3; i<end; i++){
		spdlog("%d \t%7llu \t%ld \t%ld \n", i, diff_count[i].packetsize, diff_count[i].jiffes.jiffes_sec, diff_count[i].jiffes.jiffes_usec);
		tmptime=(float)diff_count[i].jiffes.jiffes_sec+(float)diff_count[i].jiffes.jiffes_usec/(1000000.00);
		total_value+=diff_count[i].packetsize/tmptime;
	}
	sub_value = total_value/3;
	
	return (uint64_t)sub_value;
}
#if FOR_MULTI_PPPD
int get_wan_enable(int *wan_enable)
{
	int ret = 0, i = 0;
	char uiname[12] = {'\0'};
	struct ifstate_status if_status[10];
	scall_param pa = {0},pb = {0},pc = {0};

	strcpy(uiname, "ALL");
	memset(&if_status, 0, sizeof(if_status));
	ret = client_SwitchCall(IGD_MSG_IFSTATE_GET_CHECKER_RESULT,PRM_POINTER(pa, uiname, strlen(uiname)+1), PRM_POINTER(pb, &if_status, sizeof(if_status)), PRM_VALUE(pc, 10));
	if (ret < 0)
	{
		return ret;
	}
	for(i = 0; i < ret; i++)
	{
		if(!if_status[i].flag)
			continue;
		if(!strncmp(if_status[i].uiname, "WISP", 4) && !if_status[i].status)
			*wan_enable = 0;
		if(!strncmp(if_status[i].uiname, "WAN", 3) && !if_status[i].status)
		{
			*wan_enable = 1;
			break;
		}
	}
	return 0;
}
#endif
#endif

int download(struct speed_server *server)
{
	//int sizes[10] = {350, 500, 750, 1000, 1500, 2000, 2500, 3000, 3500, 4000};
	//int sizes[10] = {4000, 3500, 3000, 2500, 2000, 1500, 1000, 750, 500, 350};
	int sizes[10] = {4000, 3500, 3000, 4000, 3500, 3000, 4000, 3500, 3000, 4000};
	int i=0;
	char *pif_name = NULL;
#if FOR_MULTI_PPPD
	int ifip_num = 0, wan_enable = 1;
	int j=0, index=-1, count=0;
	struct nc_ifip ifip[RULE_MX], *p_ifip=NULL;
	
	if(get_wan_enable(&wan_enable) == 0){
		memset(&ifip, 0, sizeof(ifip));
		ifip_num = dump_ifip(ifip);
	}
#endif

	for(i=0; i<DOWN_PROCESS_NUM; i++)
	{
#if FOR_MULTI_PPPD
		if (ifip_num > 1) {
			count = 0;
			while (count++ < ifip_num) {
				if (index < 0)
					j = index = 0;
				else
					j = index;

				if (++index >= ifip_num)
					index = 0;

				p_ifip = &ifip[j];
				if (p_ifip->ifid && p_ifip->def_route && 
					((wan_enable && strcmp(p_ifip->ifname, "apclii0")) ||
					(wan_enable == 0 && !strcmp(p_ifip->ifname, "apclii0")))) 
				{
					if(strlen(p_ifip->ifname) > 0){
						igd_strcpy(speed_child_info.download_if[i], p_ifip->ifname);
						pif_name = speed_child_info.download_if[i];
					}
					break;
				}
			}
			if(!pif_name){
				speed_child_info.download_if[i][0] = '\0';
			}
		}
#endif		
		sprintf(speed_child_info.url[i], "%s/speedtest/random%dx%d.jpg", server->cut_url, sizes[i], sizes[i]);
		speed_child_info.download_pid[i] = speedtest_child(WGET_DOWNLOAD, (void *)speed_child_info.url[i], pif_name);
		spdlog("start download %d %s \n", speed_child_info.download_pid[i], pif_name?pif_name:"");
	}
	return 0;
}

void check_restart_download()
{
	int time_count = 0, i=0;
	pid_t pid;
	long diff = 0;

	for(time_count=1; time_count<DOWN_TIME; time_count++)
	{
		pid = waitpid(-1, NULL, WNOHANG);
		if(pid > 0){
			for(i=0; i<DOWN_PROCESS_NUM; i++){
				if(pid != speed_child_info.download_pid[i])
					continue;
				speed_child_info.download_pid[i] = speedtest_child(WGET_DOWNLOAD, (void *)speed_child_info.url[i], speed_child_info.download_if[i]);
				usleep(1000);
				DEBUG("restart download: %d -> %d\n", pid, speed_child_info.download_pid[i]);
				break;
			}
		}
		sleep(1);
#if ROUTER_360
		get_netdev_flowNew(&down_count[time_count], 1);
		diff = get_diff_of_count(down_count, down_diff_count, time_count);

		if( diff > 0 ){
			save_speed_result("\"status\":%d,\"band\":%ld", SPD_STATUS_RUN, diff);
		}
#endif
	}
}

void kill_download()
{
	int i=0;
	for(i=0; i<DOWN_PROCESS_NUM; i++){
		if(speed_child_info.download_pid[i] > 0){
			kill(speed_child_info.download_pid[i], SIGKILL);
			speed_child_info.download_pid[i] = 0;
		}
	}
}

int upload(struct speed_server *server)
{
	int i=0;

	for(i=0; i<UP_PROCESS_NUM; i++){
		strcpy(speed_child_info.upload_url[i], server->url);
		speed_child_info.upload_pid[i] = speedtest_child(WGET_POST_UPLOAD, speed_child_info.upload_url[i], NULL);
		spdlog("start upload %d \n", speed_child_info.upload_pid[i]);
	}
	return 0;
}

void check_restart_upload()
{
	int time_count = 0, i=0;
	pid_t pid;
	long diff = 0;

	for(time_count=1; time_count<UPLOAD_TIME; time_count++)
	{
		pid = waitpid(-1, NULL, WNOHANG);
		if(pid > 0){
			for(i=0; i<UP_PROCESS_NUM; i++){
				if(pid != speed_child_info.upload_pid[i])
					continue;
				speed_child_info.upload_pid[i] = speedtest_child(WGET_POST_UPLOAD, speed_child_info.upload_url[i], NULL);
				usleep(1000);
				DEBUG("restart upload: %d -> %d\n", pid, speed_child_info.upload_pid[i]);
				break;
			}
		}
		sleep(1);
#if ROUTER_360
		get_netdev_flowNew(&up_count[time_count], 0);
		diff = get_diff_of_count(up_count, up_diff_count, time_count);

		if( diff > 0 ){
			save_speed_result("\"status\":%d,\"band\":%llu,\"max\":%llu,\"up_band\":%llu", 
				SPD_STATUS_RUN, down_speed, down_max_speed, diff);
		}
#endif
	}
}

void kill_upload()
{
	int i=0;
	for(i=0; i<UP_PROCESS_NUM; i++){
		if(speed_child_info.upload_pid[i] > 0){
			kill(speed_child_info.upload_pid[i], SIGKILL);
			speed_child_info.upload_pid[i] = 0;
		}
	}
}

void sig_child()
{
	pid_t pid;
	while(1){
		pid = waitpid(-1, NULL, WNOHANG);
		if(pid <= 0)
			break;
		DEBUG("child %d exit \n", pid);
	}
}

void speedtest_exit()
{
#if ROUTER_360
	change_speed_result_status();
#endif
	free_server_list(&closest_server_list);
	kill_download();
	kill_upload();
	exit(0);
}

int main(int argc, char **argv)
{
	int ret = 0, test_up = 0, action = 0;
	unsigned long time_start = 0, time_end = 0;
	struct speed_server *best = NULL;

	init_server_list(&closest_server_list, CLOSEST_SERVER_NUM_MAX);
	signal(SIGINT, speedtest_exit);
	signal(SIGTERM, speedtest_exit);
	signal(SIGUSR1, speedtest_exit);

	if(argv[1]){
		if(strcmp(argv[1], "get-servers" ) == 0)	
			action = 1;
	}
	
#if ROUTER_360
	remove(SPD_DEBUG_LOG);
	if(argv[1]){
		if(strcmp(argv[1], "1" ) == 0)		// user: PC
			test_up = 1;
		else if(strcmp(argv[1], "2" ) == 0)	// user: phone
			test_up = 1;
	}
	time_start = get_system_uptime();
#else
	test_up = 1;
	get_system_uptime(&time_start);
#endif

	ret = getconfig(&g_config);
	if(ret < 0){
		spdlog("get config failed \n");
	#if ROUTER_360
		change_speed_result_status();
	#endif
		return -1;
	}
	spdlog("client: [lat: %lf] [lon: %lf] [isp: %s] \n", g_config.client_lat, g_config.client_lon, g_config.client_isp);
	
	if(access(SPEEDTEST_CLOSEST_SERVERS_XML, F_OK) == 0){
		ret = read_closest_servers_file(&closest_server_list);
		if(ret < 0){
			spdlog("read local failed, goto GET_SERVER_LIST \n");
			goto GET_SERVER_LIST;
		}
	}
	else{
GET_SERVER_LIST:
/*	#if PRE_SET_SERVERLIST_XML
		if(access(SPEEDTEST_SERVER_LIST_XML, F_OK)){
			cp_file("/sh/speedtest_server_list.xml", SPEEDTEST_SERVER_LIST_XML);
		}
	#endif */
		ret = get_server_lists(&g_config, &closest_server_list);
		if(ret < 0){
			spdlog("get server-list failed \n");
			speedtest_exit();
		}
		save_closest_servers_file(&closest_server_list);
	}
	print_server_list(&closest_server_list);
	if(action == 1)
		speedtest_exit();
	
	ret = select_best_server(&closest_server_list, &best);
	if(ret < 0 || !best){
		spdlog("select best-server failed \n");
		speedtest_exit();
	}
	spdlog("[best] distance=%lf url=%s lat=%lf lon=%lf country=%s sponsor=%s \n\n", 
			best->distance, best->url, best->lat, best->lon, best->country, best->sponsor);
#if ROUTER_360
	remove(SPEEDTEST_RESULT);
	save_speed_result("\"status\":%d,\"band\":%llu", SPD_STATUS_INIT, 0);
#endif

	spdlog("======= test download ========\n");
	download(best);
#if ROUTER_360
	sleep(1);
	get_netdev_flowNew(&down_count[0], 1);
#endif
	check_restart_download();
	kill_download();
#if ROUTER_360
	down_speed = calc_speed(down_count, down_diff_count, DOWN_TIME, &down_max_speed);
#endif

	if(test_up){
		spdlog("======= test upload ========\n");
		//sleep(1);
		upload(best);
	#if ROUTER_360
		sleep(1);
		get_netdev_flowNew(&down_count[0], 0);
	#endif
		check_restart_upload();
		kill_upload();
	#if ROUTER_360
		up_speed = calc_speed(up_count, up_diff_count, UPLOAD_TIME, &up_max_speed);
	#endif
	}

#if ROUTER_360
	time_end = get_system_uptime();
#else
	get_system_uptime(&time_end);
#endif
	
	spdlog("[time %lu s] \n", time_end - time_start);

#if ROUTER_360
	if(test_up){
		save_speed_result("\"status\":%d,\"band\":%llu,\"max\":%llu,\"up_band\":%llu", 
			SPD_STATUS_END, down_speed, down_max_speed, up_speed);
		spdlog("\"status\":%d,\"band\":%llu,\"max\":%llu,\"up_band\":%llu \n", 
			SPD_STATUS_END, down_speed, down_max_speed, up_speed);

	}
	else{
		save_speed_result("\"status\":%d,\"band\":%llu,\"max\":%llu", SPD_STATUS_END, down_speed, down_max_speed);
		spdlog("\"status\":%d,\"band\":%llu,\"max\":%llu \n", SPD_STATUS_END, down_speed, down_max_speed);
	}
#endif

	free_server_list(&closest_server_list);
	signal(SIGCHLD, sig_child);
	sleep(2);
	
	return 0;
}


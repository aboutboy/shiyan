#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <signal.h>
#include <sys/ioctl.h>
#include <linux/inotify.h>
#include <sys/socket.h>

#define IS_IN_ACCESS(mask)				((mask) & IN_ACCESS)
#define IS_IN_MODIFY(mask)				((mask) & IN_MODIFY)
#define IS_IN_ATTRIB(mask)				((mask) & IN_ATTRIB)
#define IS_IN_CLOSE_WRITE(mask)		((mask) & IN_CLOSE_WRITE)
#define IS_IN_CLOSE_NOWRITE(mask)		((mask) & IN_CLOSE_NOWRITE)
#define IS_IN_OPEN(mask)				((mask) & IN_OPEN)
#define IS_IN_MOVED_FROM(mask)		((mask) & IN_MOVED_FROM)
#define IS_IN_MOVED_TO(mask)			((mask) & IN_MOVED_TO)
#define IS_IN_CREATE(mask)				((mask) & IN_CREATE)
#define IS_IN_DELETE(mask)				((mask) & IN_DELETE)
#define IS_IN_DELETE_SELF(mask)		((mask) & IN_DELETE_SELF)
#define IS_IN_MOVE_SELF(mask)			((mask) & IN_MOVE_SELF)

char *str_inotify_mask(unsigned int mask)
{
	static char buf[32] = {'\0'};
	if(mask & IN_ACCESS)				// �ļ���ȡ
		sprintf(buf, "IN_ACCESS");
	else if(mask & IN_MODIFY)				// �ļ�д��
		sprintf(buf, "IN_MODIFY");
	else if(mask & IN_ATTRIB)				// �ļ�Ԫ����(������,Ȩ�޵�)�Ѹı�
		sprintf(buf, "IN_ATTRIB");
	else if(mask & IN_CLOSE_WRITE)				// �ļ��ѹر�������д��ģʽ��
		sprintf(buf, "IN_CLOSE_WRITE");
	else if(mask & IN_CLOSE_NOWRITE)				// �ļ��ѹر���δ����д��ģʽ��
		sprintf(buf, "IN_CLOSE_NOWRITE");
	else if(mask & IN_OPEN)				// �ļ��Ѵ�
		sprintf(buf, "IN_OPEN");
	else if(mask & IN_MOVED_FROM)				// �ļ��ѴӼ���Ŀ¼���Ƴ�
		sprintf(buf, "IN_MOVED_FROM");
	else if(mask & IN_MOVED_TO)				// �ļ����������Ŀ¼
		sprintf(buf, "IN_MOVED_TO");
	else if(mask & IN_CREATE)				// �ļ����ڼ���Ŀ¼�д���
		sprintf(buf, "IN_CREATE");
	else if(mask & IN_DELETE)				// �ļ��ѴӼ���Ŀ¼��ɾ��
		sprintf(buf, "IN_DELETE");
	else if(mask & IN_DELETE_SELF)				// ���Ӷ�������ɾ��
		sprintf(buf, "IN_DELETE_SELF");
	else if(mask & IN_MOVE_SELF)				// ���Ӷ��������Ƴ�
		sprintf(buf, "IN_MOVE_SELF");
	else
		sprintf(buf, "unkown");
	return &buf[0];
}

int get_inotify_num(int inotify_fd)
{
	unsigned int queue_len;
	int ret;
	
	ret = ioctl (inotify_fd, FIONREAD, &queue_len);
	if (ret < 0)
		return ret;
	else
		return queue_len;
}

#define INOTIFY_WFD_MAX_NUM		5
#define FSYNC_WATCH_PATH_LEN		256

struct fsync_watch
{
	int wfd;
	char path[FSYNC_WATCH_PATH_LEN];
};

int fsync_enable;
int inotify_fd;
int inotify_wfds[INOTIFY_WFD_MAX_NUM];
unsigned int inotify_watch_num;
struct fsync_watch g_watch[INOTIFY_WFD_MAX_NUM];

char *wfd_2_path(int wfd)
{
	int i=0;
	for(; i<INOTIFY_WFD_MAX_NUM; i++){
		if(wfd == g_watch[i].wfd)
			return g_watch[i].path;
	}
	return NULL;
}

void inotify_destory()
{
	int i=0;

	if(inotify_fd <= 0)
		return;
	for(; i<INOTIFY_WFD_MAX_NUM; i++){
		if(inotify_wfds[i] > 0)
			inotify_rm_watch(inotify_fd, inotify_wfds[i]);
	}
	close(inotify_fd);
	
	memset(inotify_wfds, 0, sizeof(inotify_wfds));
	inotify_fd = 0;
}

int read_inotify_fd(int fd)
{
	char buf[512] = {'\0'};
	ssize_t len, i = 0;
	struct inotify_event *event;
	
	len = read (fd, buf, 512);
	printf("read_inotify_fd  fd: %d len: %d \n", fd, len);
	while (i < len) 
	{
		event = (struct inotify_event *) &buf[i];
		printf("\n%s   %s \n", str_inotify_mask(event->mask), wfd_2_path(event->wd));
		printf("wd=%d mask=0x%x cookie=%d len=%d dir=%s\n", 
			event->wd, event->mask, event->cookie, event->len, (event->mask & IN_ISDIR) ? "yes" : "no");
		/* if there is a name, print it */
		if (event->len)
			printf("name=%s\n", event->name);
		if(IS_IN_DELETE_SELF(event->mask) || IS_IN_MOVE_SELF(event->mask)){
			fsync_enable=0;
		}
		i += sizeof (struct inotify_event) + event->len;
	}

	return len;
}

void inotify_watch()
{
	int max_fd=0, i=0;
	fd_set fds;
	struct timeval tv;

	fsync_enable=1;
	while(fsync_enable)
	{
		tv.tv_sec = 0;
		tv.tv_usec = 100000;		// 100 * 1000   100ms
		FD_ZERO(&fds);
		FD_SET(inotify_fd, &fds);
		max_fd = inotify_fd;

		if(select(max_fd+1, &fds, NULL, NULL, &tv) < 0)
		{
			if(errno == EINTR || errno == EAGAIN)
				continue;
		}

		if(FD_ISSET(inotify_fd, &fds))
			read_inotify_fd(inotify_fd);
	}
}

int fsync_init(struct fsync_watch *watch)
{
	int i=0;

	inotify_fd = inotify_init();
	if(inotify_fd < 0){
		printf("ERROR: inotify_init  %s \n", strerror(errno));
		return -1;
	}
	printf("inotify_fd: %d \n", inotify_fd);

	for(i=0; i<INOTIFY_WFD_MAX_NUM; i++){
		if(watch[i].path[0] == '\0')
			break;
		inotify_wfds[i] = inotify_add_watch(inotify_fd, watch[i].path, 
			IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF);
		if(inotify_wfds[i] < 0){
			printf("ERROR: inotify_add_watch  %s \n", strerror(errno));
			return -1;
		}
		printf("inotify_wfds: %d \n", inotify_wfds[i]);
		watch[i].wfd = inotify_wfds[i];
		++inotify_watch_num;
	}

	return 0;
}

int fsync_deal_arg(int argc, char **argv)
{
	int i=0, j=0;
	char *tmp = NULL;

	memset(g_watch, 0, sizeof(g_watch));
	if(argv[1] == NULL){
		printf("no watch path \n");
		return -1;
	}
	
	while(argv[++i]){
		strncpy(g_watch[j].path, argv[i], sizeof(g_watch[j].path)-1);
		g_watch[j].path[sizeof(g_watch[j].path)-1] = '\0';
		++j;
		if(j >= INOTIFY_WFD_MAX_NUM){
			break;
		}
	}
	return 0;
}

void fsync_exit()
{
	inotify_destory();
	exit(0);
}

int main(int argc, char **argv)
{
	int ret=0;
	signal(SIGINT, fsync_exit);/*register signal handler #include <signal.h>*/
	signal(SIGTERM, fsync_exit);/*register signal handler*/

	if(fsync_deal_arg(argc, argv) < 0)
		fsync_exit();
	if(fsync_init(g_watch) < 0)
		fsync_exit();

	inotify_watch();
	
	fsync_exit();
	return 0;
}


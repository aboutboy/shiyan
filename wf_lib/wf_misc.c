#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/ipc.h>

#include "wf_misc.h"

char wf_error[WF_ERROR_NUM_MAX][128]={
	"success",
// 1 ~ 10
	"failed",
	"param error",
	"malloc error",
	"source lack",
	"space lack",
	"open error",
	"close error",
	"read error",
	"write error",
	"unknow error"
// 11 ~ 20
};

char *wf_std_error(int *errcode)
{
	if(errcode)
		*errcode = errno;
	return strerror(errno);
}






#if 0
/*
��غ�����longjmp, siglongjmp, setjmp
��ͷ�ļ���#include <setjmp.h>
�������壺int sigsetjmp(sigjmp_buf env, int savesigs)
����˵����sigsetjmp()�ᱣ��Ŀǰ��ջ������Ȼ��Ŀǰ�ĵ�ַ��һ���Ǻţ�
���ڳ��������ط�����siglongjmp()ʱ���ֱ����������Ǻ�λ�ã�Ȼ��ԭ��ջ�����������ִ�С�
����envΪ��������Ŀǰ��ջ������һ������Ϊȫ�ֱ���
����savesigs��Ϊ��0�������õ��źż���Ҳ��һ�鱣��
��sigsetjmp()����0ʱ�����Ѿ����üǺ��ϣ������ط�0�������siglongjmp������ת������
���أ���ֱ�ӵ�����Ϊ0������siglongjmp���÷�����Ϊ��0

Ӧ��: �������alarm����ʵ�ֺ�����ʱ����
*/
static sigjmp_buf jmp_env;
static void connect_alarm(int)
{
    siglongjmp(jmp_env, 1);
}
int test_jmp()
{
	signal(SIGALRM, connect_alarm);
	if (sigsetjmp(jmp_env, 1))
	{
		printf("timeout\n");
		return 1;
	}
	alarm(3);

	sleep(5); // ִ�п��ܳ�ʱ������

	return 0;
}
#endif









void alarm_start(unsigned int seconds, void (*func)(int))
{
	signal(SIGALRM, func);
	alarm(seconds);
}

void alarm_again(unsigned int seconds)
{
	alarm(seconds);
}

void alarm_cancel()
{
	alarm(0);
}

int wf_get_selfexe_path(char *path, int len)
{
	char buf[256] = {0, };
	int ret = readlink("/proc/self/exe", buf, sizeof(buf) - 1);

	if (ret < 0)
		return ret;
	if (ret > len - 1)
		ret = len - 1;
	strncpy(path, buf, ret);
	path[ret + 1] = 0;
	return ret;
}

long wf_getsys_uptime(unsigned long *up_time)
{
/*
struct sysinfo
{
	long uptime;			// Seconds since boot 
	unsigned long loads[3];		// 1, 5, and 15 minute load averages 
	unsigned long totalram;		// Total usable main memory size 
	unsigned long freeram;		// Available memory size 
	unsigned long sharedram;	// Amount of shared memory 
	unsigned long bufferram;	// Memory used by buffers 
	unsigned long totalswap;	// Total swap space size 
	unsigned long freeswap;		// swap space still available 
	unsigned short procs;		// Number of current processes 
	unsigned short pad;		// explicit padding for m68k 
	unsigned long totalhigh;	// Total high memory size 
	unsigned long freehigh;		// Available high memory size 
	unsigned int mem_unit;		// Memory unit size in bytes 
	char _f[20-2*sizeof(long)-sizeof(int)];	// Padding: libc5 uses this.. 
};
*/
	struct sysinfo info;
	memset(&info, 0, sizeof(info));
	sysinfo(&info);
	if(up_time)
		*up_time = info.uptime;

	return info.uptime;
}

void wf_registe_exit_signal(__sighandler_t exit_call)
{
	signal(SIGINT, exit_call);/*register signal handler #include <signal.h>*/
	signal(SIGTERM, exit_call);/*register signal handler*/
	signal(SIGKILL, exit_call);/*register signal handler*/
}

// void (*exit_call)(void)
void wf_damen(__sighandler_t exit_call)
{
	if(fork()!= 0)
		exit(1);
	setsid();

	signal(SIGINT, exit_call);/*register signal handler #include <signal.h>*/
	signal(SIGTERM, exit_call);/*register signal handler*/
	signal(SIGKILL, exit_call);/*register signal handler*/
}

int getSysCmd_output(char *cmd,char *output, unsigned int size)
{
	FILE    *read_fp;
	int        chars_read = -1;
	if(cmd == NULL || output == NULL || size == 0)
		return 0;
	memset( output, 0, size );
	read_fp = popen(cmd, "r");
	if ( read_fp != NULL )
	{
		chars_read = fread(output, sizeof(char), size, read_fp);
		pclose(read_fp);
	}

	return chars_read;
}

void wf_check_exit(int semkey, char *name)
{
	char buf[32], cmd[256];
	if(semget(semkey, 1, IPC_CREAT|0666|IPC_EXCL) < 0)
	{
		if(errno == EEXIST)
		{
			memset(buf, 0 ,sizeof(buf));
			sprintf(cmd, "pidof %s |wc -w", name);
			getSysCmd_output(cmd, buf, sizeof(buf) - 1);
			if(atoi(buf) > 1)	
			{
				printf("\n%s is exist!\n", name);
				exit(-1);
			}
		}
	}
}

#if 0
void schedule()
{
	int i=1;

	setBlueWhite();
	save_cursor();
	hide_cursor();

	for(i=1;i<=100;i++)
	{
		recover_cursor();
		printf("%d%s", i, "%");
		fflush(stdout);
		usleep(100000);
	}

	default_cursor();
	show_cursor();
}

void main()
{
	schedule();
	//printf("%lu \n", get_system_uptime(NULL));
}
#endif


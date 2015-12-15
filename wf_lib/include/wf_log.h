#ifndef WF_LOG_H_
#define WF_LOG_H_

enum LOG_LEVEL
{
	LOG_OFF,		//0	// ��ߵȼ��ģ����ڹر�������־��ӡ
	LOG_FATAL,		// 1	// FATAL: ָ��ÿ�����صĴ����¼����ᵼ��Ӧ�ó�����˳�
	LOG_ERROR,		// 2	// ERROR: ָ����Ȼ���������¼�������Ȼ��Ӱ��ϵͳ�ļ�������
	LOG_WARN,		// 3	// WARN: ���������Ǳ�ڴ��������
	LOG_INFO,		// 4	// INFO: ������Ϣ�ڴ����ȼ�����ͻ��ǿ��Ӧ�ó�������й���
	LOG_DEBUG,		// 5	// DEBUG: ָ��ϸ������Ϣ�¼��Ե���Ӧ�ó����Ƿǳ��а�����
	LOG_ALL		// 6	// ��͵ȼ��ģ����ڴ�������־��ӡ
};
enum LOG_SPLIT
{
	SPLIT_OFF,		// �������־
	SPLIT_DATE,		// �������ڲ����־�ļ�
	SPLIT_LEVEL		// ������־�ȼ������־�ļ�
};

struct wf_log_cfg
{
	char log_path[128+1];				// ��־�ļ��洢·����Ĭ�ϳ���ǰĿ¼
	char logfile_suffix[5+1];			// ��־�ļ���׺����Ĭ��"log"
	enum LOG_LEVEL level_show;		// ��־��ӡ�ȼ�����
	enum LOG_SPLIT split;			// ��־�ļ���ַ�ʽ
};

enum MSG_DIRECT
{
	MSG_NO,		// �����ֽ����뷢��
	MSG_SEND,
	MSG_RECV
};

extern void wf_set_logcfg(char *log_path, char *logfile_suffix, enum LOG_LEVEL level_show, enum LOG_SPLIT split);

//extern void wf_log(char *logname, enum LOG_LEVEL loglevel, int logtime, char *logarg, char *logbuf);

extern void WFLog(char *logname, char *file, int line, enum LOG_LEVEL loglevel, char *fmt, ...);

#define WF_LOG(logname, loglevel, fmt, ...)		WFLog(logname, __FILE__, __LINE__, loglevel, fmt, ##__VA_ARGS__)

//extern void wf_msglog_note(char *logname, char *buf);

//extern void wf_msglog(char *logname, int logtime, int isAsc, unsigned char *buf, int len);

extern void NetMsgLog(char *logname, unsigned char *buf, int len, enum MSG_DIRECT msg_direct);

extern void NetMsgLogAsc(char *logname, char *buf, int len, enum MSG_DIRECT msg_direct);

extern void NetMsgLogNote(char *logname, enum MSG_DIRECT msg_direct, char *fmt, ...);

extern void wf_print(char *file, int line, enum LOG_LEVEL loglevel, char *fmt, ...);

#define WF_PRINT(loglevel, fmt, ...)	wf_print(__FILE__, __LINE__, loglevel, ">> "fmt, ##__VA_ARGS__)

#define WF_PRINT_F(loglevel, fmt, ...)	wf_print(__FILE__, 0, loglevel, ">> "fmt, ##__VA_ARGS__)

#define WF_PRINT_L(loglevel, fmt, ...)	wf_print(NULL, __LINE__, loglevel, ">> "fmt, ##__VA_ARGS__)

#define WF_PRINT_C(loglevel, fmt, ...)	wf_print(NULL, 0, loglevel, ">> "fmt, ##__VA_ARGS__)

#define pprint(fmt, ...)	printf(">> "fmt, ##__VA_ARGS__)

extern int wf_logprint_l(char *filepath, enum LOG_LEVEL loglevel, char *fmt, ...);

#define wf_logprint(filepath, fmt, ...)		wf_logprint_l(filepath, LOG_OFF, fmt, ##__VA_ARGS__)

extern int get_param_from_file(char *file, char *tag, char *out, int maxSize);

#endif


#ifndef WF_IPC_H_
#define WF_IPC_H_

typedef enum
{
	PRM_TYPE_IN_INT=1,		// ��Ҫ����һ��INT���͵�����
	PRM_TYPE_IN_PTR,		// ��Ҫ����һ��һ��ָ�룬������һ���ṹ����һ���ַ���
							// ������ַ���ָ�룬���ò�������������ַ������ȼ�1
	PRM_TYPE_OUT_PTR,		// ����Ҫ��ģ�鷵����Ϣ��ʱ��ʹ��
	PRM_TYPE_VALUE,		// value type
	PRM_TYPE_MAX,			// last one for program use,do not use this
}PRM_TYPE;


typedef struct 
{
	int type;
	unsigned long len;
	unsigned long value;/*may be a value or a pointer*/
}scall_param;

#define PARAM_MAX_NUM		4

#define PRM_IN_INT(p,v)		\
	((p).type=PRM_TYPE_IN_INT,(p).value=(v),(p).len=sizeof(int),&(p))
	
#define PRM_IN_PTR(p,v,l) \
	((p).type=PRM_TYPE_IN_PTR,(p).value=(unsigned long)(v),(p).len=(l),&(p))
	
#define PRM_OUT_PTR(p,v,l)   \
	((p).type=PRM_TYPE_OUT_PTR,(p).value=(unsigned long)(v),(p).len=(l),&(p))

#define PRM_VALUE(p,v)	\
	((p).type=PRM_TYPE_VALUE,(p).value=(unsigned long)(v),(p).len=sizeof(unsigned long),&(p))

#define PRM_VALUE_NULL(p)	\
	((p).type=PRM_TYPE_VALUE,(p).value=(unsigned long)(NULL),(p).len=sizeof(unsigned long),&(p))

typedef int IPC_SERVER_PROC_FUC(int msg, unsigned long pa, unsigned long pb, unsigned long pc, unsigned long pd);
typedef IPC_SERVER_PROC_FUC* IPC_SERVER_PROC_PTR;

extern int ipc_server_accept(int sock);

extern int ipc_server_close(char *path, int sock);

/*path must be absolute*/
extern int ipc_server_init(char *path, IPC_SERVER_PROC_PTR proc_ptr);

extern int ipc_client_call(char *server_path, int msg, int param_len, ...);

#ifndef WF_FD_SET
#define WF_FD_SET(fd, where, max_fd)		{ FD_SET(fd, where); if (fd > max_fd) max_fd = fd; }
#endif

#endif


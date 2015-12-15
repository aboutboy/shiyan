#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h> 
#include "wf_threadpool.h"

static void tp_insertEnd_job(struct job_list *list,struct job *node)
{
	if( list==NULL || node==NULL )	return;
	node->next = NULL;
	if( list->head == NULL )
	{
		list->head = node;
		node->prev = NULL;
	}
	else
	{
		list->tail->next = node;
		node->prev = list->tail;
	}
	list->tail = node;
	++list->num;
}
static struct job *tp_findJob_byID(struct job_list *list,char *jobID)
{
	struct job *tmp = NULL;
	if( list==NULL || jobID==NULL )	return NULL;
	tmp = list->head;
	while(tmp)
	{
		if( strcmp(tmp->jobID,jobID) == 0 )	return tmp;
		else tmp = tmp->next;
	}
	return NULL;
}
static void tp_removeJob(struct job_list *list,struct job *node)
{
	if(list->head == node || list->tail == node)
	{
		if(list->head == node)
		{
			list->head = node->next;
			if(list->head != NULL)	list->head->prev=NULL;
		}
		if(list->tail == node)
		{
			list->tail = node->prev;
			if(list->tail != NULL)	list->tail->next=NULL;
		}
	}
	else
	{
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}
	--list->num;
}
static void tp_deleteJob(struct job_list *list,struct job *node)
{
	tp_removeJob(list,node);
	free(node);
}
static void tp_freeJob_list(struct job_list *list)
{
	struct job *p = NULL;
	struct job *tmp = NULL;
	if( list == NULL )	return;
	p = list->head;
	while(p)
	{
		tmp = p;
		p = p->next;
		free(tmp);
	}
	list->num= 0;
	list->head= NULL;
	list->tail = NULL;
}

struct threadpool* threadpool_init(int thread_num, int queue_max_num)
{
    struct threadpool *pool = NULL;
    do 
    {
        pool = malloc(sizeof(struct threadpool));
        if (NULL == pool)
        {
            break;
        }
        pool->thread_num = thread_num;
        pool->queue_max_num = queue_max_num;
        pool->wait_list.num = 0;
        pool->wait_list.head = NULL;
        pool->wait_list.tail = NULL;
	 pool->excu_list.num = 0;
        pool->excu_list.head = NULL;
        pool->excu_list.tail = NULL;
	 pool->pend_list.num = 0;
        pool->pend_list.head = NULL;
        pool->pend_list.tail = NULL;
        if (pthread_mutex_init(&(pool->mutex), NULL))
        {
            break;
        }
        if (pthread_cond_init(&(pool->queue_empty), NULL))
        {
            break;
        }
        if (pthread_cond_init(&(pool->queue_not_empty), NULL))
        {
            break;
        }
        if (pthread_cond_init(&(pool->queue_not_full), NULL))
        {
            break;
        }
        pool->pthreads = malloc(sizeof(pthread_t) * thread_num);
        if (NULL == pool->pthreads)
        {
            break;
        }
        pool->queue_close = 0;
        pool->pool_close = 0;
        int i;
        for (i = 0; i < pool->thread_num; ++i)
        {
            pthread_create(&(pool->pthreads[i]), NULL, threadpool_function, (void *)pool);
        }
        
        return pool;    
    } while (0);
    
    return NULL;
}
struct job *threadpool_findJob_byID(struct threadpool* pool, char *jobID)
{
	struct job *pjob=NULL;
	if(pool==NULL || jobID==NULL)	return NULL;
	pthread_mutex_lock(&(pool->mutex));
	pjob = tp_findJob_byID(&(pool->excu_list), jobID);
	if(pjob==NULL)
		pjob = tp_findJob_byID(&(pool->pend_list), jobID);
	if(pjob==NULL)
		pjob = tp_findJob_byID(&(pool->wait_list), jobID);
	pthread_mutex_unlock(&(pool->mutex));
	return pjob;
}
int threadpool_add_job(struct threadpool* pool, int (*callback_function)(void *arg), void *arg, char *jobID)
{
    //assert(pool != NULL);		// ����
    //assert(callback_function != NULL);
    //assert(arg != NULL);
    struct timeval now;
    if(pool == NULL || callback_function==NULL || arg==NULL)	return -1;

    pthread_mutex_lock(&(pool->mutex));
    while ((pool->wait_list.num == pool->queue_max_num) && !(pool->queue_close || pool->pool_close))
    {
        pthread_cond_wait(&(pool->queue_not_full), &(pool->mutex));   //��������ʱ��͵ȴ�
    }
    if (pool->queue_close || pool->pool_close)    //���йرջ����̳߳عرվ��˳�
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    struct job *pjob =(struct job*) malloc(sizeof(struct job));
    if (NULL == pjob)
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    } 
	gettimeofday(&now,0);
	sprintf(pjob->jobID,"%ld%ld",now.tv_sec,now.tv_usec);
	if(jobID != NULL)	sprintf(jobID,"%s",pjob->jobID);
    pjob->callback_function = callback_function;    
	pjob->state = CREATE_JOB;
    pjob->arg = arg;
	if( pool->wait_list.head == NULL)
	{
		tp_insertEnd_job(&(pool->wait_list), pjob);
		pthread_cond_broadcast(&(pool->queue_not_empty));  //���пյ�ʱ����������ʱ��֪ͨ�̳߳��е��̣߳����зǿ�
	}
	else
	{
		tp_insertEnd_job(&(pool->wait_list), pjob);
	}
	pjob->state = WAIT_JOB;
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}
void threadpool_resumeJob(struct threadpool* pool, struct job *pjob)
{
	if(pool==NULL || pjob==NULL)	return;
	pthread_mutex_lock(&(pool->mutex));
	tp_removeJob(&(pool->pend_list), pjob);
	pjob->state = RESUME_JOB;
	if( pool->wait_list.head == NULL)
	{
		tp_insertEnd_job(&(pool->wait_list), pjob);
		pthread_cond_broadcast(&(pool->queue_not_empty));  //���пյ�ʱ����������ʱ��֪ͨ�̳߳��е��̣߳����зǿ�
	}
	else
	{
		tp_insertEnd_job(&(pool->wait_list), pjob);
	}
	pthread_mutex_unlock(&(pool->mutex));
}
int threadpool_resumeJob_byID(struct threadpool* pool, char *jobID)
{
	struct job *pjob=NULL;
	if(pool==NULL || jobID==NULL)	return -1;
	pthread_mutex_lock(&(pool->mutex));
	pjob = tp_findJob_byID(&(pool->pend_list), jobID);
	if(pjob != NULL)
	{
		tp_removeJob(&(pool->pend_list), pjob);
		pjob->state = RESUME_JOB;
		if( pool->wait_list.head == NULL)
		{
			tp_insertEnd_job(&(pool->wait_list), pjob);
			pthread_cond_broadcast(&(pool->queue_not_empty));  //���пյ�ʱ����������ʱ��֪ͨ�̳߳��е��̣߳����зǿ�
		}
		else
		{
			tp_insertEnd_job(&(pool->wait_list), pjob);
		}
	}
	pthread_mutex_unlock(&(pool->mutex));
	if(pjob != NULL)	return 0;
	else return -1;
}
void* threadpool_function(void* arg)
{
    struct threadpool *pool = (struct threadpool*)arg;
    struct job *pjob = NULL;
	int rc=0;
    while (1)  //��ѭ��
    {
        pthread_mutex_lock(&(pool->mutex));
        while ((pool->wait_list.num == 0) && !pool->pool_close)   //����Ϊ��ʱ���͵ȴ����зǿ�
        {
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->mutex));
        }
        if (pool->pool_close)   //�̳߳عرգ��߳̾��˳�
        {
            pthread_mutex_unlock(&(pool->mutex));
            pthread_exit(NULL);
        }
        pjob = pool->wait_list.head;
		tp_removeJob(&(pool->wait_list), pjob);
		pjob->state = EXCUTE_JOB;
		tp_insertEnd_job(&(pool->excu_list), pjob);
        if (pool->wait_list.num == 0)
        {
            pthread_cond_signal(&(pool->queue_empty));        //����Ϊ�գ��Ϳ���֪ͨthreadpool_destroy�����������̺߳���
        }
        if (pool->wait_list.num == pool->queue_max_num - 1)
        {
            pthread_cond_broadcast(&(pool->queue_not_full));  //���з������Ϳ���֪ͨthreadpool_add_job���������������
        }
        pthread_mutex_unlock(&(pool->mutex));
        
        rc = (*(pjob->callback_function))(pjob->arg);   //�߳�����Ҫ���Ĺ������ص������ĵ���
	if(rc == 1)
	{
		pthread_mutex_lock(&(pool->mutex));
		tp_removeJob(&(pool->excu_list), pjob);
		pjob->state = PAUSE_JOB;
		tp_insertEnd_job(&(pool->pend_list), pjob);
		pthread_mutex_unlock(&(pool->mutex));
	}
	else
	{
		pthread_mutex_lock(&(pool->mutex));
		tp_removeJob(&(pool->excu_list), pjob);
		pthread_mutex_unlock(&(pool->mutex));
		free(pjob);
	}
        pjob = NULL;    
    }
}
int threadpool_destroy(struct threadpool *pool)
{
	struct job *p;
	int i;
    //assert(pool != NULL);		// ����
    if(pool==NULL)	return -1;
	
    pthread_mutex_lock(&(pool->mutex));
    if (pool->queue_close || pool->pool_close)   //�̳߳��Ѿ��˳��ˣ���ֱ�ӷ���
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    
    pool->queue_close = 1;        //�ö��йرձ�־
    while (pool->wait_list.num != 0)
    {
        pthread_cond_wait(&(pool->queue_empty), &(pool->mutex));  //�ȴ�����Ϊ��
    }    
    
    pool->pool_close = 1;      //���̳߳عرձ�־
    pthread_mutex_unlock(&(pool->mutex));
    pthread_cond_broadcast(&(pool->queue_not_empty));  //�����̳߳��������������߳�
    pthread_cond_broadcast(&(pool->queue_not_full));   //������������threadpool_add_job����
    
    for (i = 0; i < pool->thread_num; ++i)
    {
        pthread_join(pool->pthreads[i], NULL);    //�ȴ��̳߳ص������߳�ִ�����
    }
    
    pthread_mutex_destroy(&(pool->mutex));          //������Դ
    pthread_cond_destroy(&(pool->queue_empty));
    pthread_cond_destroy(&(pool->queue_not_empty));   
    pthread_cond_destroy(&(pool->queue_not_full));    
    free(pool->pthreads);
	tp_freeJob_list(&(pool->wait_list));
	tp_freeJob_list(&(pool->excu_list));
	tp_freeJob_list(&(pool->pend_list));
    free(pool);
    return 0;
}

#if 0
struct LoadFileInfo
{
	char loadID[18];		// �ļ����ر��
	char filePath[256];		// �ļ����ش洢·��
	unsigned int fileSize;	// �ļ���С
	int loadState;		/* �ļ�����״̬��1���ȴ����أ�2���������أ�3:��ͣ���أ�4��������ɣ�
				5������ʧ�ܣ�����æ��6������ʧ�ܣ��Է��ܾ���*/
	int loadRate;		// �ļ����ؽ��ȣ����ذٷֱȵķ���
	char loadDate[12];		// �������ڣ�"yyyy-mm-dd"
	char loadTime[12];		// ����ʱ�䣬"hh:mm:ss"
	unsigned int totalTime;	// ������ʱ������λ:��
	int (*callback)(int type,void *data,int dataLen);
};
struct prt_file_t
{
	struct LoadFileInfo file;
	unsigned int ID;
	unsigned int DID;
	int prtOpt;
	//struct sockaddr_in s_addr;
};

struct threadpool *pool;
pthread_t pid;
char find_jobID[18];
char pause_jobID[18];

int asyncInform(int type,void *data,int dataLen)
{
	switch(type)
	{
		case 1:
		case 2:
			{
				struct prt_file_t *p = (struct prt_file_t *) data;
				printf("async : type==%d, %s\n",type,p->file.filePath);
			}
			break;
	}
	return 0;
}
int work(void* arg)
{
    struct prt_file_t *p = (struct prt_file_t *) arg;
    printf("threadpool callback fuction : %s.\n", p->file.filePath);
	
    sleep(10);
	free(p);
	printf("threadpool callback fuction : %s.exit\n", p->file.filePath);
	return 0;
}
int work2(void* arg)
{
    struct prt_file_t *p = (struct prt_file_t *) arg;
    printf("threadpool callback fuction : %s.\n", p->file.filePath);
	
    sleep(10);

	if(p->file.loadState == 0)
	{
		p->file.loadState = 4;
		printf("threadpool callback fuction : %s.pause\n", p->file.filePath);
		return 1;
	}
	else if(p->file.loadState == 4)
	{
		p->file.callback(1,arg,sizeof(struct prt_file_t));
		printf("threadpool callback fuction : %s.exit\n", p->file.filePath);
		free(p);
		return 0;
	}
}

void *thread_handle()
{
	int i=0;
	struct prt_file_t *p=NULL;
	char buf[1024]={'\0'};
	
	for(i=0; i<5; i++)
	{
		sprintf(buf,"%d",i+1);
		p = (struct prt_file_t *)malloc(sizeof(struct prt_file_t));
		memset(p,0,sizeof(struct prt_file_t));
		if(p==NULL)
		{
			printf("malloc failed i=%d\n",i);
			continue;
		}
		strcpy(p->file.filePath,buf);
		p->file.callback = asyncInform;

		if(i==1)
			threadpool_add_job(pool, work2, p,p->file.loadID);
		else
			threadpool_add_job(pool, work, p,p->file.loadID);
		printf("add job: %s\n",p->file.loadID);

		if(i==3)
		{
			strcpy(find_jobID,p->file.loadID);
		}
		else if(i==1)
		{
			strcpy(pause_jobID,p->file.loadID);
		}
	}
	return NULL;
}
void test_one()
{
	int i=0;
	struct prt_file_t *p=NULL;
	char buf[1024]={'\0'};
	
	for(i=0; i<40; i++)
	{
		sprintf(buf,"%d",i+1);
		p = (struct prt_file_t *)malloc(sizeof(struct prt_file_t));
		memset(p,0,sizeof(struct prt_file_t));
		if(p==NULL)
		{
			printf("malloc failed i=%d\n",i);
			continue;
		}
		strcpy(p->file.filePath,buf);

		threadpool_add_job(pool, work, p,p->file.loadID);
		printf("add job: %s\n",p->file.loadID);
	}
}
void test_two()
{
	int rc=0;
	struct job *pjob=NULL;
	struct prt_file_t *p=NULL;
	pool = threadpool_init(10, 20);
	pthread_create(&pid, NULL, thread_handle, NULL);
	sleep(2);
	pjob = threadpool_findJob_byID(pool, find_jobID);
	if(pjob != NULL)
	{
		p = (struct prt_file_t *)pjob->arg;
		printf("find job : %s\n",find_jobID);
		printf("filePath : %s\n",p->file.filePath);
	}
	else
	{
		printf("find no job : %s\n",find_jobID);
	}
}
void test_three()
{
	int rc=0;
	struct job *pjob=NULL;
	struct prt_file_t *p=NULL;
	pool = threadpool_init(10, 20);
	pthread_create(&pid, NULL, thread_handle, NULL);
	sleep(2);
	pjob = threadpool_findJob_byID(pool, find_jobID);
	if(pjob != NULL)
	{
		p = (struct prt_file_t *)pjob->arg;
		printf("find job : %s\n",find_jobID);
		printf("filePath : %s\n",p->file.filePath);
	}
	else
	{
		printf("find no job : %s\n",find_jobID);
	}
	
	sleep(8);
	rc = threadpool_resumeJob_byID(pool, pause_jobID);
	if(rc == 0)
	{
		printf("resume job ok %s\n",pause_jobID);
	}
	else
	{
		printf("resume job failed %s\n",pause_jobID);
	}
}
int main(void)
{
	//test_one();
	//test_two();
	test_three();

    sleep(10);
    threadpool_destroy(pool);
    return 0;
}
#endif


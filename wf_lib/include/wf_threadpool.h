#ifndef WF_THREADPOOL_H_
#define WF_THREADPOOL_H_

/*
	大多数的网络服务器都具有一个特点，就是单位时间内必须处理数目巨大的连接请求，
但是处理时间却是比较短的。
在传统的多线程服务器模型中是这样实现的：一旦有个请求到达，就创建一个新的线程，
由该线程执行任务，任务执行完毕之后，线程就退出。
这就是"即时创建，即时销毁"的策略。
尽管与创建进程相比，创建线程的时间已经大大的缩短，
但是如果提交给线程的任务是执行时间较短，而且执行次数非常频繁，
那么服务器就将处于一个不停的创建线程和销毁线程的状态。
这笔开销是不可忽略的，尤其是线程执行的时间非常非常短的情况。

	线程池就是为了解决上述问题的，
它的实现原理是这样的：在应用程序启动之后，就马上创建一定数量的线程，
放入空闲的队列中。这些线程都是处于阻塞状态，这些线程只占一点内存，
不占用CPU。当任务到来后，线程池将选择一个空闲的线程，将任务传入此线程中运行。
当所有的线程都处在处理任务的时候，线程池将自动创建一定的数量的新线程，
用于处理更多的任务。执行任务完成之后线程并不退出，
而是继续在线程池中等待下一次任务。当大部分线程处于阻塞状态时，
线程池将自动销毁一部分的线程，回收系统资源。

	线程池中有三个任务队列，分别是等待队列，执行队列，挂起队列。
	添加任务时，记录任务的创建时间（暂时未做）;
	挂起任务时，记录任务的挂起时间（暂时未做）;

	任务队列维护线程：
	挂起队列中的任务长时间没有被唤醒，则调用其 任务结果通知回调函数 通知任务超时，
	然后删除任务（暂时未做）

*/

#include <pthread.h>


/*任务状态*/
typedef enum
{
	CREATE_JOB,			// 创建状态
	WAIT_JOB,				// 等待执行
	EXCUTE_JOB,			// 执行状态
	PAUSE_JOB,				// 挂起
	RESUME_JOB,			// 唤醒
	FORCESTOP_JOB,			// 被迫停止
	FINISHWAIT_JOB,		// 执行完成，等待处理
	FINISH_JOB,				// 执行完成
	TIMEOUT_JOB,			// 任务超时
	DESTORY_JOB			// 销毁
}JOB_status;

struct job
{
    char jobID[18];					// 任务编号
    //time_t createTime;				// 任务创建时间，暂时不用
    //time_t pauseTime;			// 任务挂起时间，暂时不用
    //JOB_PRIORITY  priority;		// 任务优先级，暂时不用
    JOB_status state;		// 任务状态
    int (*callback_function)(void *arg);    //线程回调函数 返回值含义: 0 表示任务执行结束; 1 : 表示任务需要挂起
    void *arg;                                //回调函数参数
    struct job *next;
    struct job *prev;
};
struct job_list
{
	struct job *head;
	struct job *tail;
	int num;
};

struct threadpool
{
    int thread_num;                   //线程池中开启线程的个数
    int queue_max_num;                //等待队列中最大job的个数
    struct job_list wait_list;		// 等待队列
    struct job_list excu_list;		// 执行队列
    struct job_list pend_list;		// 挂起队列
    pthread_t *pthreads;              //线程池中所有线程的pthread_t
    pthread_mutex_t mutex;            //互斥信号量
    pthread_cond_t queue_empty;       //队列为空的条件变量
    pthread_cond_t queue_not_empty;   //队列不为空的条件变量
    pthread_cond_t queue_not_full;    //队列不为满的条件变量
    int queue_close;                  //队列是否已经关闭
    int pool_close;                   //线程池是否已经关闭
};

//================================================================================================
//函数名：                   threadpool_init
//函数描述：                 初始化线程池
//输入：                    [in] thread_num     线程池开启的线程个数
//                         [in] queue_max_num  队列的最大job个数 
//输出：                    无
//返回：                    成功：线程池地址 失败：NULL
//================================================================================================
extern struct threadpool* threadpool_init(int thread_num, int queue_max_num);

//================================================================================================
//函数名：                    threadpool_add_job
//函数描述：                  向线程池中添加任务
//输入：                     [in] pool                  线程池地址
//                          [in] callback_function     回调函数
//                          [in] arg                     回调函数参数
//输出：                     [char *] jobID	任务编号
//返回：                     成功：0 失败：-1
//================================================================================================
extern int threadpool_add_job(struct threadpool *pool, int (*callback_function)(void *arg), void *arg,char *jobID);

//================================================================================================
//函数名：                    threadpool_findJob_byID
//函数描述：                  查询任务队列中的任务
//输入：                     [in] pool                  线程池地址
//                          [char *] jobID	任务编号
//输出：                     无
//返回：                     成功：任务地址; 失败：NULL
//================================================================================================
extern struct job *threadpool_findJob_byID(struct threadpool* pool, char *jobID);

//================================================================================================
//函数名：                    threadpool_resumeJob
//函数描述：                  唤醒挂起的任务
//输入：                     [in] pool                  线程池地址
//                          [struct job *] pjob	 任务地址，该任务必须处于挂起队列中
//输出：                     无
//返回：                     无
//================================================================================================
extern void threadpool_resumeJob(struct threadpool* pool, struct job *pjob);

//================================================================================================
//函数名：                    threadpool_resumeJob_byID
//函数描述：                  根据任务编号唤醒挂起的任务
//输入：                     [in] pool                  线程池地址
//                          [char *] jobID	任务编号
//输出：                     无
//返回：                     成功：0 失败：-1
//================================================================================================
extern int threadpool_resumeJob_byID(struct threadpool* pool, char *jobID);

//================================================================================================
//函数名：                    threadpool_destroy
//函数描述：                   销毁线程池
//输入：                      [in] pool                  线程池地址
//输出：                      无
//返回：                      成功：0 失败：-1
//================================================================================================
extern int threadpool_destroy(struct threadpool *pool);

//================================================================================================
//函数名：                    threadpool_function
//函数描述：                  线程池中线程函数
//输入：                     [in] arg                  线程池地址
//输出：                     无  
//返回：                     无
//================================================================================================
extern void* threadpool_function(void* arg);

#endif


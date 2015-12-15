/*
  $Id: ac_timer.h,v 1.1 2014/07/22 02:08:13 xiaolj Exp $
  $Author: xiaolj $
  $Date: 2014/07/22 02:08:13 $
  $Log: ac_timer.h,v $
  Revision 1.1  2014/07/22 02:08:13  xiaolj
  first

*/
#ifndef __SHIYAN_TIMER__
#define  __SHIYAN_TIMER__

#define MAX_TIMER_SIZE          1024               /*����û�(��ʱ��)����*/

/********************************************************************************
 * ����     : ����һ����ʱ��������ʱִ��callBack����
 *    
 * ����     : 
 * [IN] 
 *   second - ��ʱʱ��
 *   callBack - ��ʱ�¼�������
 * [OUT]
 *   ��
 * 
 * ����ֵ   : 
 *   0  - ����
 *   ��0 - timer�ľ�����û��ر�ʱʹ��
*******************************************************************************/  
extern int timerAdd(int second,int (*callBack)(void*,int), void *user_data,int len,int if_reop);
extern void timerStop(int key);

/********************************************************************************
 * ����     : ֹͣ�����ܿ����رպ���
 *    
 * ����     : 
 * [IN] 
 *   isStart - 1��������0���ر�
 *
 * [OUT]
 *   ��
 * 
 * ����ֵ   : 
 *   OK       -   ����ɹ�
 *   ERROR    -   ����ʧ��
*******************************************************************************/  
extern int timerTaskStart(int isStart);

#endif

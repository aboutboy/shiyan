/*
*	�ַ���gb18030
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <math.h>

#undef PT_DEBUG
//#define PT_DEBUG

#ifdef PT_DEBUG
#define PLOG(fmt,...)	printf(">test> %s %d; "fmt,__FUNCTION__,__LINE__,##__VA_ARGS__)		/*c99  gcc*/
#else
#define PLOG(fmt,...)	/*nothing*/
#endif

#define PRINT_IO(fmt,...)		printf(">> "fmt,##__VA_ARGS__)
#define PRINT_ERROR(fmt,...)		printf("\terror: "fmt,##__VA_ARGS__)

#define MAX_LINE_BUF	8196

char line_buf[MAX_LINE_BUF]={'\0'};

struct stat_result
{
	int str_count;	// ��ͳ���ַ��� ���ִ���
	int word_count;	// ����
	int c_count;	// �ַ��������ƿո�
	int c_count_b;	// �ַ������ƿո�
	int para_count;	// ������
	int line_count;	// ����
	int unCh_count;	// �����ĵ���
	int space_count;
};

int stat_word_utf8(struct stat_result *result,char *data)
{
	int en=0,space_num=0,ispara=0;
	unsigned char *str = (unsigned char *)data;
	unsigned int *pw=NULL;
	if( result == NULL || str == NULL )
		return -1;
	while(*str)
	{
		if( *str >= 32 && *str <= 126 )
			PLOG("%c\n",*str);
		else
			PLOG("%02X\n",*str);
		if( *str > 0x00 && *str <= 0x7F )	// ASCII��
		{
			if( *str == 32 || *str == 9 )	// �ո�ˮƽ�Ʊ��
			{
				if( en )
				{
					result->word_count++;
					result->unCh_count++;
					en=0;
				}
				space_num++;
				result->space_count++;
			}
			else if( *str >= 33 && *str <= 126 )
			{
				ispara=1;
				en=1;
				result->c_count++;
				result->c_count_b++;
			}
		}
		else 
		{
			ispara=1;
			PLOG("utf\t");
			if( en )
			{
				result->word_count++;
				result->unCh_count++;
				en=0;
			}
			if( *str >= 0xC0 && *str <= 0xDF )	// ���ֽڱ���
			{
				if( *++str >= 0x80 )			// ��֤�Ƿ���ϱ������
				{
					PLOG("2\n");
					result->c_count++;
					result->c_count_b++;
					result->word_count++;
				}
			}
			else if( *str >= 0xE0 && *str <= 0xEF )	// ���ֽڱ���
			{
				if( *++str >= 0x80 && *++str >= 0x80 )			// ��֤�Ƿ���ϱ������
				{
					PLOG("3\n");
					result->c_count++;
					result->c_count_b++;
					result->word_count++;
				}
			}
			else if( *str >= 0xF0 && *str <= 0xF7 )	// ���ֽڱ���
			{
				if( *++str >= 0x80 && *++str >= 0x80 && *++str >= 0x80 )			// ��֤�Ƿ���ϱ������
				{
					PLOG("4\n");
					result->c_count++;
					result->c_count_b++;
					result->word_count++;
				}
			}

		}
		str++;
		
	}
	if( en )
	{
		result->word_count++;
		result->unCh_count++;
		en=0;
	}
	if( ispara )
		result->para_count++;
	result->c_count_b += space_num;
	
	return 0;
}


int stat_word(struct stat_result *result,char *data)
{
	int en=0,space_num=0,ispara=0;
	unsigned char *str = (unsigned char *)data;
	unsigned int *pw=NULL;
	if( result == NULL || str == NULL )
		return -1;
	while(*str)
	{
		if( *str >= 32 && *str <= 126 )
			PLOG("%c\n",*str);
		else
			PLOG("%02X\n",*str);
		if( *str > 0x00 && *str <= 0x7F )	// ASCII��
		{
			if( *str == 32 || *str == 9 )	// �ո�ˮƽ�Ʊ��
			{
				if( en )
				{
					result->word_count++;
					result->unCh_count++;
					en=0;
				}
				space_num++;
				result->space_count++;
			}
			else if( *str >= 33 && *str <= 126 )
			{
				ispara=1;
				en=1;
				result->c_count++;
				result->c_count_b++;
			}
		}
		else if( *str >= 0x81 && *str <= 0xFE )	// ����
		{
			ispara=1;
			PLOG("hanzi\t");
			if( en )
			{
				result->word_count++;
				result->unCh_count++;
				en=0;
			}
			result->c_count++;
			result->c_count_b++;
			str++;
			if( *str >= 0x40 && *str <= 0x7E )			// ˫�ֽں���
			{
				PLOG("2\n");
				result->word_count++;
			}
			else if( *str >= 0x80 && *str <= 0xFE )	// ˫�ֽں���
			{
				PLOG("2\n");
				result->word_count++;
			}
			else if( *str >= 0x30 && *str <= 0x39 )	// ���ֽں���
			{
				str++;
				if( *str >= 0x81 && *str <= 0xFE )
				{
					str++;
					if( *str >= 0x30 && *str <= 0x39 )
					{
						PLOG("4\n");
						result->word_count++;
					}
				}
			}

		}
		str++;
		
	}
	if( en )
	{
		result->word_count++;
		result->unCh_count++;
		en=0;
	}
	if( ispara )
		result->para_count++;
	result->c_count_b += space_num;
	
	return 0;
}

int stat(struct stat_result *result,char *path,char *str)
{
	FILE *fp;
	int count=0,buf_len=0,blank_num=0;
	char *buf = line_buf;
	char *p,*opt;

	if( result == NULL || path == NULL || str == NULL )
		return -1;
	fp = fopen(path,"r");
	if( fp == NULL )
		return -1;
	buf_len = strlen(str);
	while( fgets(buf, MAX_LINE_BUF, fp) != NULL )
	{
		stat_word(result,buf);
		result->line_count++;
		p = buf;
		while( 1 )
		{
			opt = strstr(p,str);
			if( opt == NULL )
				break;
			count++;
			p = opt + buf_len;
		}
	}
	fclose(fp);
	result->str_count = count;

	return count;
}

int file_open_able(char *path)
{
	FILE *fp;
	if( path == NULL )
		return -1;
	fp = fopen(path,"r");
	if( fp == NULL )
		return -1;
	fclose(fp);

	return 0;
}
void print_result(struct stat_result *result)
{
	printf("\n------------------ result -----------------------\n\n");
	printf("\t��ͳ���ַ�����\t\t%d\n",result->str_count);
	printf("\t����\t\t\t%d\n",result->word_count);
	printf("\t�ַ��������ƿո�\t%d\n",result->c_count);
	printf("\t�ַ������ƿո�\t%d\n",result->c_count_b);
	printf("\t������\t\t\t%d\n",result->para_count);
	printf("\t����\t\t\t%d\n",result->line_count);
	printf("\t�����ĵ���\t\t%d\n",result->unCh_count);
	printf("\n------------------ result -----------------------\n\n");
}

int main()
{
	char filepath[256]={'\0'};
	char stat_str[1024]={'\0'};
	int count=0;
	struct stat_result result;
	memset(&result,0,sizeof(struct stat_result));
	/*char data[1024]="fsfe fa����d f";
	char buf[1024]="fsfe fa����d f";
	stat_word(&result,data);
	print_result(&result);
	PLOG("space_count=%d\n",result.space_count);
	stat_word(&result,buf);
	print_result(&result);
	PLOG("space_count=%d\n",result.space_count);*/
	
	while(1)
	{
		memset(&result,0,sizeof(struct stat_result));
		memset(filepath,0,256);
		memset(stat_str,0,1024);
		PRINT_IO("ѡ��txt�ı��ĵ�:");
		gets(filepath);
		if( file_open_able(filepath) != 0 )
		{
			PRINT_ERROR("file can't open !\n");
			continue;
		}
		PRINT_IO("������Ҫͳ�Ƶ��ַ���:");
		gets(stat_str);
		count=stat(&result,filepath,stat_str);
		print_result(&result);
		PLOG("space_count=%d\n",result.space_count);
	}

	return 0;
}

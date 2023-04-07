#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log()
{
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}
//�첽��Ҫ�����������еĳ��ȣ�ͬ������Ҫ����
//��ʼ��������־�ļ��Լ�һЩ��־�ļ�����Ϣ
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //���������max_queue_size,������Ϊ�첽
    if (max_queue_size >= 1)
    {
        //�첽
        m_is_async = true;
        //�����������в������������г���
        m_log_queue = new block_queue<string>(max_queue_size);
        //�������߳�
        pthread_t tid;
        //flush_log_threadΪ�ص�����,�����ʾ�����߳��첽д��־
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }
    
    m_close_log = close_log;
    //�����С��buf���ݳ�ʼ��
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    //�������
    m_split_lines = split_lines;

    //ʱ��
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    //�Ӻ���ǰ�ҵ���һ��/��λ��
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    //�൱���Զ�����־��
    //��������ļ���û��/����ֱ�ӽ�ʱ��+�ļ�����Ϊ��־��
    if (p == NULL)
    {
        snprintf(log_full_name, 1024, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 1024, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;
    
    //����־�ļ�
    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

//���ݲ�ͬ���ļ��߼�д�벻ͬ����־��Ϣ
void Log::write_log(int level, const char *format, ...)
{
    //��ȡд���ʱ��
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    //��¼�ַ���
    char s[16] = {0};

    //��־�ּ� 0-debug 1-info 2-warn 3-erro
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    //д��һ��log����m_count++, m_split_lines�������
    m_mutex.lock();
    m_count++;//��־����+1

    //��־���ǽ����д�����־����������еı���
    //m_split_linesΪ�������
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
       
        //��ʽ����־���е�ʱ�䲿��
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
       
        //�����ʱ�䲻�ǽ���,�򴴽��������־������m_today��m_count
        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 1024, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            //����������У���֮ǰ����־�������ϼӺ�׺, m_count/m_split_lines
            snprintf(new_log, 1024, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
 
    m_mutex.unlock();

    va_list valst;
    //�������format������ֵ��valst�����ڸ�ʽ�����
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    //д��ľ���ʱ�����ݸ�ʽ:ʱ���������
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    //���ݸ�ʽ�����������ַ����д�ӡ���ݡ����ݸ�ʽ�û��Զ��壬����д�뵽�ַ�����str�е��ַ�����(��������ֹ��)
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();
    //���첽,����־��Ϣ������������,ͬ����������ļ���д
    if (m_is_async && !m_log_queue->full()){
        m_log_queue->push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush(void)
{
    m_mutex.lock();
    //ǿ��ˢ��д����������
    fflush(m_fp);
    m_mutex.unlock();
}

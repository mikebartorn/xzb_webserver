#pragma once 

#include<iostream>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>
#include<string.h>
#include<pthread.h>
#include<sys/mman.h>
#include<errno.h>
#include<stdarg.h>
#include<sys/uio.h>
#include<mysql/mysql.h>
#include<fstream>
#include<map>
#include"lst_timer.h"
#include"threadpool.h"
#include"log.h"
#include"block_queue.h"
#include"utils.h"
#include"sql_connection_pool.h"

using namespace std;

const bool ET = true;
#define TIMESLOT 5 //��ʱ����ʱ��5��

class util_timer;//ǰ������
class sort_timer_lst;

class http_con {
public:

    http_con();//���캯��
    ~http_con();//��������
    void process();//����ͻ�������
    bool read();//������������
    bool write();//������д����
    void init(int sockfd, const sockaddr_in& addr, int close_log, 
                string user, string passwd, string basename);//��ʼ������
    void close_con();//�ر�����

    sockaddr_in *get_address(){
        return &m_address;
    }

    void initmysql_result(Connection_pool *connPool);//

public:
    static int m_user_count;//�ͻ������ӵ�����
    static int m_epollfd;//����socket�ϵ��¼�����ע�ᵽͬһ��epoll�ں��¼��У��������óɾ�̬��
    static const int READ_BUFFER_SIZE = 2048;//����������С
    static const int WRITE_BUFFER_SIZE = 2048;//д��������С
    static const int FILENAME_LEN = 1024;//�ļ����Ƴ���

    int m_close_log;

    util_timer* timer;
    static sort_timer_lst m_timer_lst;// ��ʱ������

    Utils util;

    MYSQL* mysql;   //���ݿ�

    // HTTP���󷽷�������ֻ֧��GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
        �����ͻ�������ʱ����״̬����״̬
        CHECK_STATE_REQUESTLINE:��ǰ���ڷ���������
        CHECK_STATE_HEADER:��ǰ���ڷ���ͷ���ֶ�
        CHECK_STATE_CONTENT:��ǰ���ڽ���������
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    /*
        ����������HTTP����Ŀ��ܽ�������Ľ����Ľ��
        NO_REQUEST          :   ������������Ҫ������ȡ�ͻ�����
        GET_REQUEST         :   ��ʾ�����һ����ɵĿͻ�����
        BAD_REQUEST         :   ��ʾ�ͻ������﷨����
        NO_RESOURCE         :   ��ʾ������û����Դ
        FORBIDDEN_REQUEST   :   ��ʾ�ͻ�����Դû���㹻�ķ���Ȩ��
        FILE_REQUEST        :   �ļ�����,��ȡ�ļ��ɹ�
        INTERNAL_ERROR      :   ��ʾ�������ڲ�����
        CLOSED_CONNECTION   :   ��ʾ�ͻ����Ѿ��ر�������
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    
    // ��״̬�������ֿ���״̬�����еĶ�ȡ״̬���ֱ��ʾ
    // 1.��ȡ��һ���������� 2.�г��� 3.���������Ҳ�����
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

private:
    
    //����process_read�����Է���http����
    HTTP_CODE process_read();//����http������
    void init();//��ʼ��������Ϣ
    HTTP_CODE parse_request_line(char* text);//��������������
    HTTP_CODE parse_headers(char* text);//��������ͷ������
    HTTP_CODE parse_content(char* text);//������������
    char* get_line() { return m_read_buf + m_start_line; }//�Ӷ��������л�ȡһ�е�����
    LINE_STATUS parse_line();//����һ�����ݣ��ж��Ƿ�����
    HTTP_CODE do_request();//��ȡ����һ����ȷ��������HTTP���󣬽��з���

    //http��Ӧ����
    bool process_write(HTTP_CODE ret);
    void unmap();
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_content_type();
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();

private:
    //��http���ӵĿͻ��˵�socket�Ͷ�Ӧ�ĵ�ַ
    int m_sockfd;
    sockaddr_in m_address;
private :
    char m_read_buf[READ_BUFFER_SIZE];  //��������
    int m_read_index;                   //�������ж���ͻ������ݵ�λ��
    int m_checked_index;                //��ǰ���ڷ������ַ��ڶ��������е�λ��
    int m_start_line;                   //��ǰ���ڽ������е���ʼλ��

    CHECK_STATE m_check_state;          //��״̬����ǰ������״̬
    METHOD m_method;                    //����ķ���

    char m_write_buf[WRITE_BUFFER_SIZE];//д������
    int m_write_idx;                    // д�������д����͵��ֽ���
    char* m_file_address;               // �ͻ������Ŀ���ļ���mmap���ڴ��е���ʼλ��
    struct stat m_file_stat;            // Ŀ���ļ���״̬��ͨ�������ǿ����ж��ļ��Ƿ���ڡ��Ƿ�ΪĿ¼���Ƿ�ɶ�������ȡ�ļ���С����Ϣ
    struct iovec m_iv[2];               // ���ǽ�����writev��ִ��д���������Զ�������������Ա������m_iv_count��ʾ��д�ڴ���������
    int m_iv_count;

    int bytes_to_send;                  //��Ҫ���͵����ݵ��ֽ���
    int bytes_have_send;               //�Ѿ����͵��ֽ���

    char m_real_file[FILENAME_LEN];     // �ͻ������Ŀ���ļ�������·���������ݵ��� doc_root + m_url, doc_root����վ��Ŀ¼
    char* m_url;                        //�ͻ������Ŀ���ļ����ļ�����index.html
    char* m_version;                    //httpЭ��İ汾�ţ���֧��1.1
    char* m_host;                       //������
    int m_content_length;               //http������Ϣ���ܳ���
    bool m_linger;                      //http�����Ƿ�Ҫ�󱣳�����

    char *m_string;                     //�洢����ͷ���ݣ��û��������룩
    int cgi;                            //�Ƿ�ʹ��POST


    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];      
};



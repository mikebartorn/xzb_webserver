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
#define TIMESLOT 5 //定时周期时间5秒

class util_timer;//前向声明
class sort_timer_lst;

class http_con {
public:

    http_con();//构造函数
    ~http_con();//析构函数
    void process();//处理客户端请求
    bool read();//非阻塞读数据
    bool write();//非阻塞写数据
    void init(int sockfd, const sockaddr_in& addr, int close_log, 
                string user, string passwd, string basename);//初始化连接
    void close_con();//关闭连接

    sockaddr_in *get_address(){
        return &m_address;
    }

    void initmysql_result(Connection_pool *connPool);//

public:
    static int m_user_count;//客户端连接的数量
    static int m_epollfd;//所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
    static const int READ_BUFFER_SIZE = 2048;//读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 2048;//写缓冲区大小
    static const int FILENAME_LEN = 1024;//文件名称长度

    int m_close_log;

    util_timer* timer;
    static sort_timer_lst m_timer_lst;// 定时器链表

    Utils util;

    MYSQL* mysql;   //数据库

    // HTTP请求方法，这里只支持GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    
    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

private:
    
    //用于process_read函数以分析http请求
    HTTP_CODE process_read();//解析http请求函数
    void init();//初始化连接信息
    HTTP_CODE parse_request_line(char* text);//处理请求行数据
    HTTP_CODE parse_headers(char* text);//处理请求头部数据
    HTTP_CODE parse_content(char* text);//处理请求内容
    char* get_line() { return m_read_buf + m_start_line; }//从读缓冲区中获取一行的数据
    LINE_STATUS parse_line();//解析一行数据，判断是否完整
    HTTP_CODE do_request();//获取到了一个正确的完整的HTTP请求，进行分析

    //http响应函数
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
    //该http连接的客户端的socket和对应的地址
    int m_sockfd;
    sockaddr_in m_address;
private :
    char m_read_buf[READ_BUFFER_SIZE];  //读缓冲区
    int m_read_index;                   //缓冲区中读入客户端数据的位置
    int m_checked_index;                //当前正在分析的字符在读缓冲区中的位置
    int m_start_line;                   //当前正在解析的行的起始位置

    CHECK_STATE m_check_state;          //主状态机当前所处的状态
    METHOD m_method;                    //请求的方法

    char m_write_buf[WRITE_BUFFER_SIZE];//写缓冲区
    int m_write_idx;                    // 写缓冲区中待发送的字节数
    char* m_file_address;               // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;            // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];               // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;

    int bytes_to_send;                  //将要发送的数据的字节数
    int bytes_have_send;               //已经发送的字节数

    char m_real_file[FILENAME_LEN];     // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char* m_url;                        //客户请求的目标文件的文件名称index.html
    char* m_version;                    //http协议的版本号，仅支持1.1
    char* m_host;                       //主机名
    int m_content_length;               //http请求消息的总长度
    bool m_linger;                      //http请求是否要求保持连接

    char *m_string;                     //存储请求头数据（用户名和密码）
    int cgi;                            //是否使用POST


    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];      
};



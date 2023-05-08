#pragma once
#include<iostream>
#include<signal.h>
#include<string.h>
#include<exception>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<unistd.h>
#include <assert.h>
#include"locker.h"
#include"threadpool.h"
#include"http_con.h"
#include"lst_timer.h"
#include"utils.h"
#include"config.h"
#include"log.h"
#include"block_queue.h"
#include"lst_timer.h"
#include"sql_connection_pool.h"

using namespace std;

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000


class Webserver {
public:

    Webserver();
    ~Webserver();

    //初始化函数
    void init(int port, int close_log, string user, string passward, string basename, 
                int max_conn);
    //线程池
    void thread_pool();
    //socket监听
    void eventlisten();
    //运行程序
    void eventloop();
    //处理管道写事件函数
    bool dealsignal(bool &timeout, bool &stop_over);
    //处理socket监听事件函数
    bool dealclient();
    
    //日志函数
    void log_write();
    //数据库初始化函数
    void sql_init();

public:
    //端口
    int m_port;

    //线程相关变量
    threadpool<http_con>* pool = NULL;

    //监听变量
    int m_listenfd;                   //监听fd
    int reuse;                      //端口复用
    struct sockaddr_in serveradd;   //服务端地址信息
    
    //epoll事件相关
    epoll_event events[MAX_EVENT_NUMBER];   //epoll监听事件
    int m_epollfd;                            //epoll文件描述符

    //http_con连接相关
    http_con *users;

    //log日志
    int m_close_log;

    //utils
    Utils utils;

    //数据库相关
    string m_user;      //登录名称
    string m_passward;  //登录密码
    string m_basename;  //数据库名称
    int m_max_conn;     //数据库最大连接数量
    Connection_pool* m_connpool; //数据库池
    
    int pipefd[2];   //管道文件描述符 0为读,1为写
};


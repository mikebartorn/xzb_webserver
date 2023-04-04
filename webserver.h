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
#include"./lock/locker.h"
#include"./threadpool/threadpool.h"
#include"./http_con/http_con.h"
#include"./timer/lst_timer.h"
#include"./utils/utils.h"
#include"./config/config.h"

using namespace std;

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000


class Webserver {
public:

    Webserver();
    ~Webserver();

    //初始化函数
    void init(int port);
    //线程池
    void thread_pool();
    //socket监听
    void eventlisten();
    //运行程序
    void eventloop();

public:
    //端口
    int port;

    //线程相关变量
    threadpool<http_con>* pool = NULL;

    //监听变量
    int listenfd;                   //监听fd
    int reuse;                      //端口复用
    struct sockaddr_in serveradd;   //服务端地址信息
    
    //epoll事件相关
    epoll_event events[MAX_EVENT_NUMBER];   //epoll监听事件
    int epollfd;                            //epoll文件描述符

    //http_con连接相关
    http_con *users;

    //utils
    Utils utils;


    int pipefd[2];   //管道文件描述符 0为读,1为写
};


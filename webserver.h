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
#include"./log/log.h"
#include"./log/block_queue.h"
#include"./timer/lst_timer.h"

using namespace std;

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000


class Webserver {
public:

    Webserver();
    ~Webserver();

    //��ʼ������
    void init(int port, int close_log);
    //�̳߳�
    void thread_pool();
    //socket����
    void eventlisten();
    //���г���
    void eventloop();
    //����ܵ�д�¼�����
    bool dealsignal(bool &timeout, bool &stop_over);
    //����socket�����¼�����
    bool dealclient();
    
    //��־����
    void log_write();

public:
    //�˿�
    int m_port;

    //�߳���ر���
    threadpool<http_con>* pool = NULL;

    //��������
    int m_listenfd;                   //����fd
    int reuse;                      //�˿ڸ���
    struct sockaddr_in serveradd;   //����˵�ַ��Ϣ
    
    //epoll�¼����
    epoll_event events[MAX_EVENT_NUMBER];   //epoll�����¼�
    int m_epollfd;                            //epoll�ļ�������

    //http_con�������
    http_con *users;

    //log��־
    int m_close_log;

    //utils
    Utils utils;

    //��ʱ��
    
    int pipefd[2];   //�ܵ��ļ������� 0Ϊ��,1Ϊд
};


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

    //��ʼ������
    void init(int port, int close_log, string user, string passward, string basename, 
                int max_conn);
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
    //���ݿ��ʼ������
    void sql_init();

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

    //���ݿ����
    string m_user;      //��¼����
    string m_passward;  //��¼����
    string m_basename;  //���ݿ�����
    int m_max_conn;     //���ݿ������������
    Connection_pool* m_connpool; //���ݿ��
    
    int pipefd[2];   //�ܵ��ļ������� 0Ϊ��,1Ϊд
};


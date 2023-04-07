#pragma once

#include<sys/epoll.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<fcntl.h>
#include<unistd.h>
#include<signal.h>
#include<assert.h>
#include<string.h>
#include<errno.h>

class Utils{
public:

    Utils();
    ~Utils();
public:
    //��epollfd��Ӽ����ļ�������
    void addfd(int epollfd, int fd, bool one_shoot, bool et);
    //��epollʵ����ɾ����Ҫ�������ļ�������
    void removefd(int epollfd, int fd);
    //��epollʵ�����޸���Ҫ�������ļ�������
    void modfd(int epollfd, int fd, int ev);
    //���ļ�����������Ϊ������
    void setnonblock(int fd);

    // ��ܵ�д���ݵ��źŲ�׽�ص�����
    static void sig_to_pipe(int sig);
    //��׽�źź���
    void addsig(int sig, void(handeler) (int));
public:
    static int* u_pipefd;
};


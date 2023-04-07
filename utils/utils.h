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
    //往epollfd添加监听文件描述符
    void addfd(int epollfd, int fd, bool one_shoot, bool et);
    //往epoll实例中删除需要监听的文件描述符
    void removefd(int epollfd, int fd);
    //往epoll实例中修改需要监听的文件描述符
    void modfd(int epollfd, int fd, int ev);
    //将文件描述符设置为非阻塞
    void setnonblock(int fd);

    // 向管道写数据的信号捕捉回调函数
    static void sig_to_pipe(int sig);
    //捕捉信号函数
    void addsig(int sig, void(handeler) (int));
public:
    static int* u_pipefd;
};


#include "utils.h"

Utils::Utils() {

}

Utils::~Utils() {

}

int Utils::u_pipefd[2] = {0, 0};

//往epoll中添加需要监听的文件描述符
void Utils::addfd(int epollfd, int fd, bool one_shoot, bool et) {
    //配置文件描述符信息
    struct epoll_event epev;
    epev.data.fd = fd;
    if (et) {
        epev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }else {
        epev.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shoot) {//防止同一个通信被不同的线程处理
        epev.events |= EPOLLONESHOT;
    }
    //往epollfd添加监听的文件描述符
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &epev);
    //设置为非阻塞形式
    setnonblock(fd);
}

//往epollfd删除文件描述符
void Utils::removefd(int epollfd, int fd) {
    //往epollfd中删除文件描述符
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    //关闭文件描述符
    close(fd);
}

//修改epollfd中fd的状态
void Utils::modfd(int epollfd, int fd, int ev) {
    struct epoll_event epev;
    epev.data.fd = fd;
    epev.events = ev | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &epev);
}

//设置文件描述符为非阻塞形式
void Utils::setnonblock(int fd) {
    //获取文件描述的状态
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}   

void Utils::addsig(int sig, void(handeler) (int)) {
    struct sigaction sa;
    memset( &sa, '\0', sizeof(sa));
    sa.sa_handler = handeler;
    //清空临时阻塞信号集
    sigfillset(&sa.sa_mask);
    //注册信号捕捉
    assert (sigaction(sig, &sa, NULL) != -1);
}

void Utils::sig_to_pipe(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

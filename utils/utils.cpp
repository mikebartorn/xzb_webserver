#include "utils.h"

Utils::Utils() {

}

Utils::~Utils() {

}

int Utils::u_pipefd[2] = {0, 0};

//��epoll�������Ҫ�������ļ�������
void Utils::addfd(int epollfd, int fd, bool one_shoot, bool et) {
    //�����ļ���������Ϣ
    struct epoll_event epev;
    epev.data.fd = fd;
    if (et) {
        epev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }else {
        epev.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shoot) {//��ֹͬһ��ͨ�ű���ͬ���̴߳���
        epev.events |= EPOLLONESHOT;
    }
    //��epollfd��Ӽ������ļ�������
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &epev);
    //����Ϊ��������ʽ
    setnonblock(fd);
}

//��epollfdɾ���ļ�������
void Utils::removefd(int epollfd, int fd) {
    //��epollfd��ɾ���ļ�������
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    //�ر��ļ�������
    close(fd);
}

//�޸�epollfd��fd��״̬
void Utils::modfd(int epollfd, int fd, int ev) {
    struct epoll_event epev;
    epev.data.fd = fd;
    epev.events = ev | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &epev);
}

//�����ļ�������Ϊ��������ʽ
void Utils::setnonblock(int fd) {
    //��ȡ�ļ�������״̬
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}   

void Utils::addsig(int sig, void(handeler) (int)) {
    struct sigaction sa;
    memset( &sa, '\0', sizeof(sa));
    sa.sa_handler = handeler;
    //�����ʱ�����źż�
    sigfillset(&sa.sa_mask);
    //ע���źŲ�׽
    assert (sigaction(sig, &sa, NULL) != -1);
}

void Utils::sig_to_pipe(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

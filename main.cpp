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

using namespace std;

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000

static int pipefd[2];   //�ܵ��ļ������� 0Ϊ��,1Ϊд

//��epollʵ���������Ҫ�������ļ�������
extern void addfd(int epollfd, int fd, bool one_shoot, bool et);
//��epollʵ����ɾ����Ҫ�������ļ�������
extern void removefd(int epollfd, int fd);
//��epollʵ�����޸���Ҫ�������ļ�������
extern void modfd(int epollfd, int fd);
//���ļ�����������Ϊ������
extern void setnonblock(int fd);

// ��ܵ�д���ݵ��źŲ�׽�ص�����
void sig_to_pipe(int sig){
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

//��׽SIGPIPE�ź�
void addsig(int sig, void(handeler) (int)) {
    struct sigaction sa;
    memset( &sa, '\0', sizeof(sa));
    sa.sa_handler = handeler;
    //�����ʱ�����źż�
    sigfillset(&sa.sa_mask);
    //ע���źŲ�׽
    assert (sigaction(sig, &sa, NULL) != -1);
}

//������
int main(int argc, char* argv[]) {

    //ȷ������˿ں�
    if (argc <= 1) {
        cout<<"please input port number"<<endl;
        exit(-1);
    }
    
    //ת���˿ں�Ϊint����
    int port = atoi(argv[1]);
    //��׽SIGPIPE�ź�,�����ź�
    addsig(SIGPIPE, SIG_IGN);

    //�����̳߳�
    threadpool<http_con>* pool = NULL;
    try {
        //��Ҫ�ֶ��ͷ��ڴ�
        pool = new threadpool<http_con>;
    }catch (...) {
        exit(-1);
    }

    //socket��������̣�ʹ��TCP���ӣ��߳�ʹ��epoll��·����
    //����socket����
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    //���ö˿ڸ���
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int ret = 0;
    //�����������˿���Ϣ
    struct sockaddr_in serveradd;
    serveradd.sin_addr.s_addr = INADDR_ANY;
    serveradd.sin_family = AF_INET;
    serveradd.sin_port = htons(port);
    //�������󶨶˿���Ϣ
    ret = bind(listenfd, (struct sockaddr*)&serveradd, sizeof(serveradd));
    assert(ret != -1);
    //����������
    ret = listen(listenfd, 5);
    assert(ret != -1);

    //����epoll����
    int epollfd = epoll_create(5);
    //��Ҫ�������¼��������ж��
    epoll_event events[MAX_EVENT_NUMBER];
    //��Ӽ����ļ���������epollʵ����
    addfd(epollfd, listenfd, false, false);

    //�����ͻ���http���ӣ���Ҫ�ֶ��ͷ��ڴ�
    http_con* users = new http_con[MAX_FD];
    http_con::m_epollfd = epollfd;

    //�����ܵ�
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblock(pipefd[1]);//��д�ܵ�����Ϊ������

    //��׽��ʱ�ź�
    addsig(SIGALRM, sig_to_pipe);   //��ʱ�ź�
    addsig(SIGTERM, sig_to_pipe);   //SIGTERM�رշ�����
    bool stop_server = false;       //�رշ�������־

    //��ʱ����ʱ�䵽��
    bool timeout = false;
    //��ʱ����SIGALARM�ź�
    alarm(TIMESLOT);  


    while (!stop_server) {
        //epoll_wait��ȡ�¼�������fd
        int epollret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((epollret < 0) && (errno != EINTR)) {
            cout<<"epoll failed!"<<endl;
            break;
        }
        //������ȡ���ķ������ļ�������
        for (int i = 0; i < epollret; i++) {
            //��ȡfd
            int sockfd = events[i].data.fd;
            //�ж��Ǽ������ļ����������ǿͻ��˵��ļ�������
            if (sockfd == listenfd) {
                //����Ǽ�������������Ҫ��ӵ�epoll����������
                struct sockaddr_in clientadd;
                socklen_t len = sizeof(clientadd);
                int cfd = accept(listenfd, (struct sockaddr *)&clientadd, &len);
                //�ж��Ƿ�ɹ�
                if (cfd == -1) {
                    cout<<"errno is "<<errno<<endl;
                    continue;
                }
                //�жϿͻ����Ƿ�ﵽ�����������
                if (http_con::m_user_count >= MAX_FD) {
                    close(cfd);
                    continue;
                }
                //�����ͻ����µ�����
                users[cfd].init(cfd, clientadd);
            }else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret == -1){
                    continue;
                }else if(ret == 0){
                    continue;
                }else{
                    for(int i = 0; i < ret; ++i){
                        switch (signals[i]){
                        case SIGALRM:
                        // ��timeout��������ж�ʱ������Ҫ����������������ʱ����
                        // ������Ϊ��ʱ��������ȼ����Ǻܸߣ��������ȴ�����������Ҫ������
                            timeout = true;
                            break;
                        case SIGTERM:
                            stop_server = true;
                        }
                    }
                }
            }else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //��ⷢ���¼����󣬹رտͻ�������
                users[sockfd].close_con();
                http_con::m_timer_lst.del_timer(users[sockfd].timer);//�Ƴ���Ӧ�Ķ�ʱ��
            }else if (events[i].events & EPOLLIN) {//��⵽���¼�
                //�����ݣ���ȡ���ݣ������ݹرտͻ�������
                if (users[sockfd].read()) {
                    pool->append(users+sockfd);
                }else {
                    users[sockfd].close_con();
                    http_con::m_timer_lst.del_timer(users[sockfd].timer);//�Ƴ���Ӧ�Ķ�ʱ��
                }
            }else if (events[i].events & EPOLLOUT) {//��⵽д�¼�
                //д�¼���֮��رտͻ�������
                if (!users[sockfd].write()) {
                    users[sockfd].close_con();
                    http_con::m_timer_lst.del_timer(users[sockfd].timer);//�Ƴ���Ӧ�Ķ�ʱ��
                }
            }
        }
        if(timeout) {
            // ��ʱ��������ʵ���Ͼ��ǵ���tick()����
            http_con::m_timer_lst.tick();
            // ��Ϊһ�� alarm ����ֻ������һ��SIGALARM �źţ���������Ҫ���¶�ʱ���Բ��ϴ��� SIGALARM�źš�
            alarm(TIMESLOT);
            timeout = false;    // ����timeout
        }
    }
    //�ر�epoll���
    close(epollfd);
    //�رշ���˼���fd
    close(listenfd);
    //�ͷ�
    delete[] users;
    delete pool;
    exit(-1);
}


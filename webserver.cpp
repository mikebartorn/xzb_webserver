#include "webserver.h"

Webserver::Webserver() {

}

Webserver::~Webserver() {
    //�ر�epoll���
    close(epollfd);
    //�رշ���˼���fd
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    //�ͷ�
    delete[] users;
    delete pool;
}


void Webserver::init(int p) {
    port = p;
}

//�����̳߳أ�ʹ��Ĭ���߳�����
void Webserver::thread_pool() {
    try {
        //��Ҫ�ֶ��ͷ��ڴ�
        pool = new threadpool<http_con>;
    }catch (...) {
        exit(-1);
    }
}

void Webserver::eventlisten() {
    //socket��������̣�ʹ��TCP���ӣ��߳�ʹ��epoll��·����
    //����socket����
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    //���ö˿ڸ���
    reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int ret = 0;
    //�����������˿���Ϣ
    serveradd.sin_addr.s_addr = INADDR_ANY;
    serveradd.sin_family = AF_INET;
    serveradd.sin_port = htons(port);
    //�������󶨶˿���Ϣ
    ret = bind(listenfd, (struct sockaddr*)&serveradd, sizeof(serveradd));
    assert(ret != -1);
    //����������
    ret = listen(listenfd, 5);
    assert(ret != -1);

    //epoll�����ں��¼���
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    //��Ӽ����ļ���������epollʵ����
    utils.addfd(epollfd, listenfd, false, false);

    //�����ͻ���http���ӣ���Ҫ�ֶ��ͷ��ڴ�
    users = new http_con[MAX_FD];
    http_con::m_epollfd = epollfd;

    //�����ܵ�
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    utils.setnonblock(pipefd[1]);//��д�ܵ�����Ϊ������

    //��׽SIGPIPE�ź�,�����ź�
    utils.addsig(SIGPIPE, SIG_IGN);
    
    //��׽��ʱ�ź�
    utils.addsig(SIGALRM, utils.sig_to_pipe);   //��ʱ�ź�
    utils.addsig(SIGTERM, utils.sig_to_pipe);   //SIGTERM�رշ�����

    //��ʱ����ʱ�䵽��
    bool timeout = false;
    //��ʱ����SIGALARM�ź�
    alarm(TIMESLOT);
}

void Webserver::eventloop() {
    bool timeout = false;
    bool stop_server = false;
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
                bool flag = dealclient();
                if (!flag) cout<<"dealclient false"<<endl;
            }else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                //�����ź�
                bool flag = dealsignal(timeout, stop_server);
                if (!flag) cout<<"dealsignal false!"<<endl; 
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
}

bool Webserver::dealclient(){
    //����Ǽ�������������Ҫ��ӵ�epoll����������
    struct sockaddr_in clientadd;
    socklen_t len = sizeof(clientadd);
    int cfd = accept(listenfd, (struct sockaddr *)&clientadd, &len);
    //�ж��Ƿ�ɹ�
    if (cfd == -1) {
        cout<<"errno is "<<errno<<endl;
        return false;
    }
    //�жϿͻ����Ƿ�ﵽ�����������
    if (http_con::m_user_count >= MAX_FD) {
        close(cfd);
        return false;
    }
    //�����ͻ����µ�����
    users[cfd].init(cfd, clientadd);
    return true;
}

bool Webserver::dealsignal(bool &timeout, bool &stop_server) {
    int sig;
    char signals[1024];
    int ret = recv(pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1){
        return false;
    }else if(ret == 0){
        return false;
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
}

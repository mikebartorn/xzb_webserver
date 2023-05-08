#include "../include/webserver.h"

Webserver::Webserver() {
    
}

Webserver::~Webserver() {
    //�ر�epoll���
    close(m_epollfd);
    //�رշ���˼���fd
    close(m_listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    //�ͷ�
    delete[] users;
    delete pool;
}


void Webserver::init(int port, int close_log, string user, string passward, string basename,
                        int max_conn) {
    m_port = port;
    m_close_log = close_log;
    m_user = user;
    m_passward = passward;
    m_basename = basename;
    m_max_conn = max_conn;
}

void Webserver::sql_init() {
    m_connpool = Connection_pool::GetInstance();
    m_connpool->init("localhost", m_user, m_passward, m_basename, 3306, 
                        m_max_conn, m_close_log);

    //��ʼ�����ݿ��ȡ��
    users->initmysql_result(m_connpool);
}

//�����̳߳أ�ʹ��Ĭ���߳�����
void Webserver::thread_pool() {
    //��Ҫ�ֶ��ͷ��ڴ�
    pool = new threadpool<http_con>(m_connpool);
}

void Webserver::eventlisten() {
    //socket��������̣�ʹ��TCP���ӣ��߳�ʹ��epoll��·����
    //����socket����
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //���ö˿ڸ���
    reuse = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int ret = 0;
    //�����������˿���Ϣ
    serveradd.sin_addr.s_addr = INADDR_ANY;
    serveradd.sin_family = AF_INET;
    serveradd.sin_port = htons(m_port);
    //�������󶨶˿���Ϣ
    ret = bind(m_listenfd, (struct sockaddr*)&serveradd, sizeof(serveradd));
    assert(ret != -1);
    //����������
    ret = listen(m_listenfd, 5);
    assert(ret != -1);

    //epoll�����ں��¼���
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    //��Ӽ����ļ���������epollʵ����
    utils.addfd(m_epollfd, m_listenfd, false, false);

    //�����ͻ���http���ӣ���Ҫ�ֶ��ͷ��ڴ�
    users = new http_con[MAX_FD];
    http_con::m_epollfd = m_epollfd;

    //�����ܵ�
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    utils.setnonblock(pipefd[1]);//��д�ܵ�����Ϊ������
    utils.addfd(m_epollfd, pipefd[0], false, false);
    Utils::u_pipefd = pipefd;
    
    //��׽SIGPIPE�ź�,�����ź�
    utils.addsig(SIGPIPE, SIG_IGN);
    //��׽��ʱ�ź�
    utils.addsig(SIGALRM, utils.sig_to_pipe);   //��ʱ�ź�
    utils.addsig(SIGTERM, utils.sig_to_pipe);   //SIGTERM�رշ�����
    
    //��ʱ����SIGALARM�ź�
    alarm(TIMESLOT);
}

void Webserver::eventloop() {
    bool timeout = false;
    bool stop_server = false;
    while (!stop_server) {
        //epoll_wait��ȡ�¼�������fd
        int epollret = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((epollret < 0) && (errno != EINTR)) {
            LOG_ERROR("%s", "epoll failure!");
            break;
        }
        //������ȡ���ķ������ļ�������
        for (int i = 0; i < epollret; i++) {
            //��ȡfd
            int sockfd = events[i].data.fd;
            //�ж��Ǽ������ļ����������ǿͻ��˵��ļ�������
            if (sockfd == m_listenfd) {
                bool flag = dealclient();
                if (!flag) LOG_ERROR("%s", "dealclient failure!");
            
            }else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                //�����ź�
                bool flag = dealsignal(timeout, stop_server);
                if (!flag) LOG_ERROR("%s", "dealsignal failure!");

            }else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //��ⷢ���¼����󣬹رտͻ�������
                users[sockfd].close_con();
                http_con::m_timer_lst.del_timer(users[sockfd].timer);//�Ƴ���Ӧ�Ķ�ʱ��

            }else if (events[i].events & EPOLLIN) {//��⵽���¼�
                //�����ݣ���ȡ���ݣ������ݹرտͻ�������
                if (users[sockfd].read()) {
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
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
                LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            }
        }
        if(timeout) {
            // ��ʱ��������ʵ���Ͼ��ǵ���tick()����
            http_con::m_timer_lst.tick();
            // ��Ϊһ�� alarm ����ֻ������һ��SIGALARM �źţ���������Ҫ���¶�ʱ���Բ��ϴ��� SIGALARM�źš�
            alarm(TIMESLOT);
            LOG_INFO("%s", "timer tick");
            timeout = false;    // ����timeout
        }
    }
}

bool Webserver::dealclient(){
    //����Ǽ�������������Ҫ��ӵ�epoll����������
    struct sockaddr_in clientadd;
    socklen_t len = sizeof(clientadd);
    int cfd = accept(m_listenfd, (struct sockaddr *)&clientadd, &len);
    //�ж��Ƿ�ɹ�
    if (cfd == -1) {
        LOG_ERROR("%s:errno is:%d", "accept errno", errno);
        return false;
    }
    //�жϿͻ����Ƿ�ﵽ�����������
    if (http_con::m_user_count >= MAX_FD) {
        close(cfd);
        return false;
    }
    //�����ͻ����µ�����
    users[cfd].init(cfd, clientadd, m_close_log, m_user, m_passward, m_basename);
    return true;
    LOG_INFO("deal client %s", inet_ntoa(clientadd.sin_addr));
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
                case SIGALRM:{
                    // ��timeout��������ж�ʱ������Ҫ����������������ʱ����
                    // ������Ϊ��ʱ��������ȼ����Ǻܸߣ��������ȴ�����������Ҫ������
                    timeout = true;
                    break;
                }
                
                case SIGTERM:{
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void Webserver::log_write() {
    if (m_close_log == 0) {
        Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
    }
}

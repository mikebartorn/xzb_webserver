#include "webserver.h"

Webserver::Webserver() {

}

Webserver::~Webserver() {
    //关闭epoll检测
    close(epollfd);
    //关闭服务端监听fd
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    //释放
    delete[] users;
    delete pool;
}


void Webserver::init(int p) {
    port = p;
}

//创建线程池，使用默认线程数量
void Webserver::thread_pool() {
    try {
        //需要手动释放内存
        pool = new threadpool<http_con>;
    }catch (...) {
        exit(-1);
    }
}

void Webserver::eventlisten() {
    //socket服务端流程，使用TCP连接，线程使用epoll多路复用
    //创建socket对象
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    //设置端口复用
    reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int ret = 0;
    //创建服务器端口信息
    serveradd.sin_addr.s_addr = INADDR_ANY;
    serveradd.sin_family = AF_INET;
    serveradd.sin_port = htons(port);
    //服务器绑定端口信息
    ret = bind(listenfd, (struct sockaddr*)&serveradd, sizeof(serveradd));
    assert(ret != -1);
    //监听服务器
    ret = listen(listenfd, 5);
    assert(ret != -1);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    //添加监听文件描述符到epoll实例中
    utils.addfd(epollfd, listenfd, false, false);

    //创建客户端http连接，需要手动释放内存
    users = new http_con[MAX_FD];
    http_con::m_epollfd = epollfd;

    //创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    utils.setnonblock(pipefd[1]);//将写管道设置为非阻塞

    //捕捉SIGPIPE信号,忽略信号
    utils.addsig(SIGPIPE, SIG_IGN);
    
    //捕捉定时信号
    utils.addsig(SIGALRM, utils.sig_to_pipe);   //定时信号
    utils.addsig(SIGTERM, utils.sig_to_pipe);   //SIGTERM关闭服务器

    //定时周期时间到了
    bool timeout = false;
    //定时产生SIGALARM信号
    alarm(TIMESLOT);
}

void Webserver::eventloop() {
    bool timeout = false;
    bool stop_server = false;
    while (!stop_server) {
        //epoll_wait获取事件发生的fd
        int epollret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((epollret < 0) && (errno != EINTR)) {
            cout<<"epoll failed!"<<endl;
            break;
        }
        //遍历获取到的发生的文件描述符
        for (int i = 0; i < epollret; i++) {
            //获取fd
            int sockfd = events[i].data.fd;
            //判断是监听的文件描述符还是客户端的文件描述符
            if (sockfd == listenfd) {
                bool flag = dealclient();
                if (!flag) cout<<"dealclient false"<<endl;
            }else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                //处理信号
                bool flag = dealsignal(timeout, stop_server);
                if (!flag) cout<<"dealsignal false!"<<endl; 
            }else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //检测发现事件错误，关闭客户端连接
                users[sockfd].close_con();
                http_con::m_timer_lst.del_timer(users[sockfd].timer);//移除对应的定时器
            }else if (events[i].events & EPOLLIN) {//检测到读事件
                //有数据，读取数据，无数据关闭客户端连接
                if (users[sockfd].read()) {
                    pool->append(users+sockfd);
                }else {
                    users[sockfd].close_con();
                    http_con::m_timer_lst.del_timer(users[sockfd].timer);//移除对应的定时器
                }
            }else if (events[i].events & EPOLLOUT) {//检测到写事件
                //写事件，之后关闭客户端连接
                if (!users[sockfd].write()) {
                    users[sockfd].close_con();
                    http_con::m_timer_lst.del_timer(users[sockfd].timer);//移除对应的定时器
                }
            }
        }
        if(timeout) {
            // 定时处理任务，实际上就是调用tick()函数
            http_con::m_timer_lst.tick();
            // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
            alarm(TIMESLOT);
            timeout = false;    // 重置timeout
        }
    }
}

bool Webserver::dealclient(){
    //如果是监听描述符，需要添加到epoll监听队列中
    struct sockaddr_in clientadd;
    socklen_t len = sizeof(clientadd);
    int cfd = accept(listenfd, (struct sockaddr *)&clientadd, &len);
    //判断是否成功
    if (cfd == -1) {
        cout<<"errno is "<<errno<<endl;
        return false;
    }
    //判断客户端是否达到最大连接数量
    if (http_con::m_user_count >= MAX_FD) {
        close(cfd);
        return false;
    }
    //建立客户端新的连接
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
            // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
            // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
            }
        }
    }
}

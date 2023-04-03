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

static int pipefd[2];   //管道文件描述符 0为读,1为写

//往epoll实例中添加需要监听的文件描述符
extern void addfd(int epollfd, int fd, bool one_shoot, bool et);
//往epoll实例中删除需要监听的文件描述符
extern void removefd(int epollfd, int fd);
//往epoll实例中修改需要监听的文件描述符
extern void modfd(int epollfd, int fd);
//将文件描述符设置为非阻塞
extern void setnonblock(int fd);

// 向管道写数据的信号捕捉回调函数
void sig_to_pipe(int sig){
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

//捕捉SIGPIPE信号
void addsig(int sig, void(handeler) (int)) {
    struct sigaction sa;
    memset( &sa, '\0', sizeof(sa));
    sa.sa_handler = handeler;
    //清空临时阻塞信号集
    sigfillset(&sa.sa_mask);
    //注册信号捕捉
    assert (sigaction(sig, &sa, NULL) != -1);
}

//主函数
int main(int argc, char* argv[]) {

    //确保输入端口号
    if (argc <= 1) {
        cout<<"please input port number"<<endl;
        exit(-1);
    }
    
    //转化端口号为int整型
    int port = atoi(argv[1]);
    //捕捉SIGPIPE信号,忽略信号
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池
    threadpool<http_con>* pool = NULL;
    try {
        //需要手动释放内存
        pool = new threadpool<http_con>;
    }catch (...) {
        exit(-1);
    }

    //socket服务端流程，使用TCP连接，线程使用epoll多路复用
    //创建socket对象
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    //设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int ret = 0;
    //创建服务器端口信息
    struct sockaddr_in serveradd;
    serveradd.sin_addr.s_addr = INADDR_ANY;
    serveradd.sin_family = AF_INET;
    serveradd.sin_port = htons(port);
    //服务器绑定端口信息
    ret = bind(listenfd, (struct sockaddr*)&serveradd, sizeof(serveradd));
    assert(ret != -1);
    //监听服务器
    ret = listen(listenfd, 5);
    assert(ret != -1);

    //创建epoll对象
    int epollfd = epoll_create(5);
    //需要监听的事件，可能有多个
    epoll_event events[MAX_EVENT_NUMBER];
    //添加监听文件描述符到epoll实例中
    addfd(epollfd, listenfd, false, false);

    //创建客户端http连接，需要手动释放内存
    http_con* users = new http_con[MAX_FD];
    http_con::m_epollfd = epollfd;

    //创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblock(pipefd[1]);//将写管道设置为非阻塞

    //捕捉定时信号
    addsig(SIGALRM, sig_to_pipe);   //定时信号
    addsig(SIGTERM, sig_to_pipe);   //SIGTERM关闭服务器
    bool stop_server = false;       //关闭服务器标志

    //定时周期时间到了
    bool timeout = false;
    //定时产生SIGALARM信号
    alarm(TIMESLOT);  


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
                //如果是监听描述符，需要添加到epoll监听队列中
                struct sockaddr_in clientadd;
                socklen_t len = sizeof(clientadd);
                int cfd = accept(listenfd, (struct sockaddr *)&clientadd, &len);
                //判断是否成功
                if (cfd == -1) {
                    cout<<"errno is "<<errno<<endl;
                    continue;
                }
                //判断客户端是否达到最大连接数量
                if (http_con::m_user_count >= MAX_FD) {
                    close(cfd);
                    continue;
                }
                //建立客户端新的连接
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
                        // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                        // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                            timeout = true;
                            break;
                        case SIGTERM:
                            stop_server = true;
                        }
                    }
                }
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
    //关闭epoll检测
    close(epollfd);
    //关闭服务端监听fd
    close(listenfd);
    //释放
    delete[] users;
    delete pool;
    exit(-1);
}


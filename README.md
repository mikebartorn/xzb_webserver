# webserver
linux下的c++轻量级web服务器

1.使用线程池和非阻塞的socket+epoll+模拟proactor的并发模型
2.使用状态机解析HTTP请求报文，支持解析get和post请求
3.访问服务器数据集实现web客户端注册、登录功能
4.实现日志系统，记录服务器运行状态
5.webbench压力测试可以实现上万的并发连接数据交换



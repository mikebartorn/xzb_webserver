#pragma once 
#include "webserver.h"

//解析main函数的输入参数
class Config {
public:
    //构造函数
    Config();
    ~Config();
    //参数转化
    void parse_arg(int argc, char* argv[]);
public:
    int port;       //端口号
    int close_log;  //是否关闭日志
    int max_conn;//数据库最大值
};


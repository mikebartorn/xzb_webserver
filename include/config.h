#pragma once 
#include "webserver.h"

//����main�������������
class Config {
public:
    //���캯��
    Config();
    ~Config();
    //����ת��
    void parse_arg(int argc, char* argv[]);
public:
    int port;       //�˿ں�
    int close_log;  //�Ƿ�ر���־
    int max_conn;//���ݿ����ֵ
};


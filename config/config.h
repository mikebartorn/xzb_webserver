#pragma once 
#include <unistd.h>
#include <stdlib.h>

//����main�������������
class Config {
public:
    //���캯��
    Config();
    ~Config();
    //����ת��
    void parse_arg(int argc, char* argv[]);
public:
    int port;//�˿ں�
};


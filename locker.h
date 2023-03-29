#pragma once

#include<pthread.h>
#include<exception>
#include<semaphore.h>
#include <stdio.h>
#include <assert.h>

using namespace std;

class locker{
public:
    //��ʼ��������
    locker();
    //�����������ͷŻ�����
    ~locker();
    //����������
    bool lock();
    //����������
    bool unlock();
    //��ȡ������
    pthread_mutex_t* get();

private:
    pthread_mutex_t m_mutex;//������
};

//��������
class cond{
public:
    //���캯������ʼ����������
    cond();
    //�����������ͷ���������
    ~cond();
    //�ȴ���������������
    bool wait(pthread_mutex_t* m_mutex);
    //ʱ��ȴ�����������
    bool timedwait(pthread_mutex_t* m_mutex, struct timespec t);
    //���Ѻ���������һ�����߶���߳�
    bool signal();
    //�������еȴ��Ľ���
    bool broadcast();

private:
    pthread_cond_t m_cond;
};

//�ź���
class sem{
public:
    //Ĭ�Ϲ��캯��
    sem();
    //�вι��캯��
    sem(int num);
    //��������
    ~sem();
    //�ȴ��������ź���-1
    bool wait();
    //���Ѻ������ź���+1
    bool post();
private:
    sem_t m_sem;//�ź���
};


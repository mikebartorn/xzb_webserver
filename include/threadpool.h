#pragma once

#include <pthread.h>
#include <exception>
#include <iostream>
#include <list>
#include <stdio.h>
#include "locker.h"
#include "sql_connection_pool.h"

using namespace std;

template<typename T>
class threadpool {
public:
    //���캯��
    threadpool(Connection_pool *connpool, int thread_number = 8, int max_requests = 10000);
    //��������
    ~threadpool();
    //��ӹ������е����������
    bool append(T* request);

private:
    //���̴߳�����
    static void* worker(void* arg);
    //��������
    void run();
    
private:
    //�̵߳��������
    int m_pthreadnum;
    //����̵߳�����
    pthread_t* m_pthread;
    //������е��������
    int m_queuesize;
    //�������
    list<T*> m_queue;
    //������
    locker m_queue_lock;
    //�ź���
    sem m_queue_sem;
    //�Ƿ�ֹͣ�߳�
    bool m_stop;

    Connection_pool* m_connpool;//���ݿ��
};


template<typename T>
threadpool<T>:: threadpool(Connection_pool *connpool, int thread_number, int max_requests):m_connpool(connpool), 
    m_pthreadnum(thread_number),m_queuesize(max_requests), m_stop(false), m_pthread(NULL){
    if((m_pthreadnum <= 0) || (m_queuesize <= 0) ) {
        throw std::exception();
        exit(-1);
    }
    //��������̵߳�����
    m_pthread = new pthread_t[m_pthreadnum];
    if(!m_pthread) {
        throw std::exception();
        exit(-1);
    }
    //�����߳�
    for (int i = 0; i < thread_number; i++) {
        // cout<<"create the"<<i<<"th thread"<<endl;
        if(pthread_create(m_pthread+i, NULL, worker, this) != 0) {
            delete[] m_pthread;
            throw std::exception();
            exit(-1);
        }
        if(pthread_detach(m_pthread[i]) != 0) {
            delete[] m_pthread;
            throw std::exception();
            exit(-1);
        }
    }
}

//��������
template<typename T>
threadpool<T>:: ~threadpool() {
    delete[] m_pthread;
    m_stop = true;
}

//�̳߳��������������ӹ����Ķ��У����ʱ��Ҫ��������Ϊ����������ǹ������Դ
//���������е���һ������ͬʱ��һ���ź�����������������Ƿ�����Դ��Ҳ���ǹ������У�
template<typename T>
bool threadpool<T>::append(T* request) {
    //����������
    m_queue_lock.lock();
    //�������������ˣ��ͷ���false�����ҽ���
    if (m_queue.size() > m_queuesize) {
        m_queue_lock.unlock();
        return false;
    }
    //û����������������ӵ����������
    m_queue.push_back(request);
    //�Ի���������
    m_queue_lock.unlock();
    //�ź�����1
    m_queue_sem.post();
    return true;
}

//���̴߳�����
template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool *)arg;
    pool->run();
}

//����������Ҳ���ǽ���������й���ȡ������
template<typename T>
void threadpool<T>::run() {
    while (!m_stop) {
        //�ȶ��ź�������,Ҳ�����ź���-1
        m_queue_sem.wait();
        //�ٶԻ���������
        m_queue_lock.lock();
        //�ж϶����Ƿ�Ϊ��
        if (m_queue.empty()) {
            m_queue_lock.unlock();
            continue;
        }
        //ȡ��������еĹ�������
        T* request = m_queue.front();
        m_queue.pop_front();
        //�Ի���������
        m_queue_lock.unlock();
        ConnectionRAII mysqlcon(&request->mysql, m_connpool);
        request->process();
    }
}

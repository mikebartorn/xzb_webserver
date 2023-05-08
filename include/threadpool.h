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
    //构造函数
    threadpool(Connection_pool *connpool, int thread_number = 8, int max_requests = 10000);
    //析构函数
    ~threadpool();
    //添加工作队列到请求队列中
    bool append(T* request);

private:
    //子线程处理函数
    static void* worker(void* arg);
    //工作函数
    void run();
    
private:
    //线程的最大数量
    int m_pthreadnum;
    //存放线程的数组
    pthread_t* m_pthread;
    //请求队列的最大数量
    int m_queuesize;
    //请求队列
    list<T*> m_queue;
    //互斥量
    locker m_queue_lock;
    //信号量
    sem m_queue_sem;
    //是否停止线程
    bool m_stop;

    Connection_pool* m_connpool;//数据库池
};


template<typename T>
threadpool<T>:: threadpool(Connection_pool *connpool, int thread_number, int max_requests):m_connpool(connpool), 
    m_pthreadnum(thread_number),m_queuesize(max_requests), m_stop(false), m_pthread(NULL){
    if((m_pthreadnum <= 0) || (m_queuesize <= 0) ) {
        throw std::exception();
        exit(-1);
    }
    //创建存放线程的数组
    m_pthread = new pthread_t[m_pthreadnum];
    if(!m_pthread) {
        throw std::exception();
        exit(-1);
    }
    //创建线程
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

//析构函数
template<typename T>
threadpool<T>:: ~threadpool() {
    delete[] m_pthread;
    m_stop = true;
}

//线程池往请求队列中添加工作的队列，添加时需要加锁，因为请求队列算是共享的资源
//存放请求队列的是一个链表，同时有一个信号量描述请求队列中是否有资源（也就是工作队列）
template<typename T>
bool threadpool<T>::append(T* request) {
    //互斥量上锁
    m_queue_lock.lock();
    //如果请求队列满了，就返回false，并且解锁
    if (m_queue.size() > m_queuesize) {
        m_queue_lock.unlock();
        return false;
    }
    //没满，将工作队列添加到请求队列中
    m_queue.push_back(request);
    //对互斥量解锁
    m_queue_lock.unlock();
    //信号量加1
    m_queue_sem.post();
    return true;
}

//子线程处理函数
template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool *)arg;
    pool->run();
}

//工作函数，也就是将请求队列中工作取出处理
template<typename T>
void threadpool<T>::run() {
    while (!m_stop) {
        //先对信号量加锁,也就是信号量-1
        m_queue_sem.wait();
        //再对互斥量加锁
        m_queue_lock.lock();
        //判断队列是否为空
        if (m_queue.empty()) {
            m_queue_lock.unlock();
            continue;
        }
        //取出请求队列的工作函数
        T* request = m_queue.front();
        m_queue.pop_front();
        //对互斥量解锁
        m_queue_lock.unlock();
        ConnectionRAII mysqlcon(&request->mysql, m_connpool);
        request->process();
    }
}

#pragma once

#include<pthread.h>
#include<exception>
#include<semaphore.h>
#include <stdio.h>
#include <assert.h>

using namespace std;

class locker{
public:
    //初始化互斥量
    locker();
    //析构函数，释放互斥量
    ~locker();
    //互斥量加锁
    bool lock();
    //互斥量解锁
    bool unlock();
    //获取互斥量
    pthread_mutex_t* get();

private:
    pthread_mutex_t m_mutex;//互斥量
};

//条件变量
class cond{
public:
    //构造函数，初始化条件变量
    cond();
    //析构函数，释放条件变量
    ~cond();
    //等待函数，调用阻塞
    bool wait(pthread_mutex_t* m_mutex);
    //时间等待，调用阻塞
    bool timedwait(pthread_mutex_t* m_mutex, struct timespec t);
    //唤醒函数，唤醒一个或者多个线程
    bool signal();
    //唤醒所有等待的进程
    bool broadcast();

private:
    pthread_cond_t m_cond;
};

//信号量
class sem{
public:
    //默认构造函数
    sem();
    //有参构造函数
    sem(int num);
    //析构函数
    ~sem();
    //等待函数，信号量-1
    bool wait();
    //唤醒函数，信号量+1
    bool post();
private:
    sem_t m_sem;//信号量
};


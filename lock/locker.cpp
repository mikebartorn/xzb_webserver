#include "locker.h"

//初始化互斥量
locker::locker(){
    pthread_mutex_init(&m_mutex, NULL);
}
//析构函数，释放互斥量
locker::~locker(){
    pthread_mutex_destroy(&m_mutex);
}
//互斥量加锁
bool locker::lock() {
    return pthread_mutex_lock(&m_mutex) == 0;
}
//互斥量解锁
bool locker::unlock() {
    return pthread_mutex_unlock(&m_mutex) == 0;
}

//获取互斥量
pthread_mutex_t* locker:: get() {
    return &m_mutex;
}

 //构造函数，初始化条件变量
cond::cond() {
    pthread_cond_init(&m_cond, NULL);
}
//析构函数，释放条件变量
cond::~cond() {
    pthread_cond_destroy(&m_cond);
}
//等待函数，调用阻塞
bool cond::wait(pthread_mutex_t* m_mutex) {
    return pthread_cond_wait(&m_cond, m_mutex) == 0;
}
//时间等待，调用阻塞
bool cond::timedwait(pthread_mutex_t* m_mutex, struct timespec t) {
    return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
}
//唤醒函数，唤醒一个或者多个线程
bool cond::signal() {
    return pthread_cond_signal(&m_cond) == 0;
}
//唤醒所有等待的进程
bool cond::broadcast() {
    return pthread_cond_broadcast(&m_cond) == 0;
}

//默认构造函数
sem::sem() {
    assert(sem_init(&m_sem, 0, 0) == 0);
}
//有参构造函数
sem::sem(int num) {
    sem_init(&m_sem, 0, num);
}
//析构函数
sem::~sem() {
    sem_destroy(&m_sem);
}
//等待函数，信号量-1
bool sem::wait() {
    return sem_wait(&m_sem) == 0;
}
//唤醒函数，信号量+1
bool sem::post() {
    return sem_post(&m_sem) == 0;
}


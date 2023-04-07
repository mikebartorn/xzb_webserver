#include "locker.h"

//��ʼ��������
locker::locker(){
    pthread_mutex_init(&m_mutex, NULL);
}
//�����������ͷŻ�����
locker::~locker(){
    pthread_mutex_destroy(&m_mutex);
}
//����������
bool locker::lock() {
    return pthread_mutex_lock(&m_mutex) == 0;
}
//����������
bool locker::unlock() {
    return pthread_mutex_unlock(&m_mutex) == 0;
}

//��ȡ������
pthread_mutex_t* locker:: get() {
    return &m_mutex;
}

 //���캯������ʼ����������
cond::cond() {
    pthread_cond_init(&m_cond, NULL);
}
//�����������ͷ���������
cond::~cond() {
    pthread_cond_destroy(&m_cond);
}
//�ȴ���������������
bool cond::wait(pthread_mutex_t* m_mutex) {
    return pthread_cond_wait(&m_cond, m_mutex) == 0;
}
//ʱ��ȴ�����������
bool cond::timedwait(pthread_mutex_t* m_mutex, struct timespec t) {
    return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
}
//���Ѻ���������һ�����߶���߳�
bool cond::signal() {
    return pthread_cond_signal(&m_cond) == 0;
}
//�������еȴ��Ľ���
bool cond::broadcast() {
    return pthread_cond_broadcast(&m_cond) == 0;
}

//Ĭ�Ϲ��캯��
sem::sem() {
    assert(sem_init(&m_sem, 0, 0) == 0);
}
//�вι��캯��
sem::sem(int num) {
    sem_init(&m_sem, 0, num);
}
//��������
sem::~sem() {
    sem_destroy(&m_sem);
}
//�ȴ��������ź���-1
bool sem::wait() {
    return sem_wait(&m_sem) == 0;
}
//���Ѻ������ź���+1
bool sem::post() {
    return sem_post(&m_sem) == 0;
}


#pragma once

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include "http_con.h"
#include "locker.h"

class http_con;
// ��ʱ����
class util_timer {
public:
    util_timer() : prev(NULL), next(NULL){}

public:
   time_t expire;   // ����ʱʱ�䣬����ʹ�þ���ʱ��
   http_con* user_data; 
   util_timer* prev;    // ָ��ǰһ����ʱ��
   util_timer* next;    // ָ���һ����ʱ��
};

// ��ʱ����������һ������˫�������Ҵ���ͷ�ڵ��β�ڵ㡣
class sort_timer_lst {
public:
    sort_timer_lst() : head( NULL ), tail( NULL ) {}
    // ��������ʱ��ɾ���������еĶ�ʱ��
    ~sort_timer_lst() {
        util_timer* tmp = head;
        while( tmp ) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    
    // ��Ŀ�궨ʱ��timer��ӵ�������
    void add_timer( util_timer* timer ); 
    
    /* ��ĳ����ʱ�������仯ʱ��������Ӧ�Ķ�ʱ���������е�λ�á��������ֻ���Ǳ������Ķ�ʱ����
    ��ʱʱ���ӳ�����������ö�ʱ����Ҫ�������β���ƶ���*/
    void adjust_timer(util_timer* timer);
   
    // ��Ŀ�궨ʱ�� timer ��������ɾ��
    void del_timer( util_timer* timer );  

    /* SIGALARM �ź�ÿ�α������������źŴ�������ִ��һ�� tick() �������Դ��������ϵ�������*/
    void tick(); 

private:
    /* һ�����صĸ����������������е� add_timer ������ adjust_timer ��������
    �ú�����ʾ��Ŀ�궨ʱ�� timer ��ӵ��ڵ� lst_head ֮��Ĳ��������� */
    void add_timer(util_timer* timer, util_timer* lst_head); 

private:
    util_timer* head;   // ͷ���
    util_timer* tail;   // β���
};

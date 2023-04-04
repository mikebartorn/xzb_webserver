#include "lst_timer.h"

// ��Ŀ�궨ʱ��timer��ӵ�������
void sort_timer_lst::add_timer( util_timer* timer ) {
    if( !timer ) {
        cout<<"timer null"<<endl;
        return;
    }
    if( !head ) {       // ��ӵ�Ϊ��һ���ڵ㣬ͷ��㣨β�ڵ㣩
        head = tail = timer;

    }
    // Ŀ�궨ʱ���ĳ�ʱʱ����С����Ѹö�ʱ����������ͷ��,��Ϊ�����µ�ͷ�ڵ�
    else if( timer->expire < head->expire ) {
        timer->next = head;
        head->prev = timer;
        head = timer;
    }
    // ����������غ�������������head�ڵ�֮����ʵ�λ�ã��Ա�֤�������������
    else{
        add_timer(timer, head);  
    }
    // // http_conn::m_timer_lst_locker.unlock();
}

/* ��ĳ����ʱ�������仯ʱ��������Ӧ�Ķ�ʱ���������е�λ�á�
�������ֻ���Ǳ������Ķ�ʱ���ĳ�ʱʱ���ӳ�����������ö�ʱ����Ҫ�������β���ƶ���*/
void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if( !timer )  {
        cout<<"timer null"<<endl;
        return;
    }
    util_timer* tmp = timer->next;
    // �����������Ŀ�궨ʱ�����������β�������߸ö�ʱ���µĳ�ʱʱ��ֵ��ȻС������һ����ʱ���ĳ�ʱʱ�����õ���
    if( !tmp || ( timer->expire < tmp->expire ) ) {
        // return;
    }
    // ���Ŀ�궨ʱ���������ͷ�ڵ㣬�򽫸ö�ʱ����������ȡ�������²�������
    else if( timer == head ) {
        head = head->next;          // ȡ��ͷ���
        head->prev = NULL;
        timer->next = NULL;
        add_timer( timer, head );   // ���¼���
    } else {
        // ���Ŀ�궨ʱ�����������ͷ�ڵ㣬�򽫸ö�ʱ����������ȡ����Ȼ�������ԭ������λ�ú�Ĳ���������
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer( timer, timer->next );
    }
}

/* һ�����صĸ����������������е� add_timer ������ adjust_timer ��������
�ú�����ʾ��Ŀ�궨ʱ�� timer ��ӵ��ڵ� lst_head ֮��Ĳ��������� */
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)  {
    util_timer* prev = lst_head;
    util_timer* tmp = prev->next;
    /* ���� list_head �ڵ�֮��Ĳ�������ֱ���ҵ�һ����ʱʱ�����Ŀ�궨ʱ���ĳ�ʱʱ��ڵ�
    ����Ŀ�궨ʱ������ýڵ�֮ǰ */
    while(tmp) {
        if( timer->expire < tmp->expire ) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    /* ��������� lst_head �ڵ�֮��Ĳ���������δ�ҵ���ʱʱ�����Ŀ�궨ʱ���ĳ�ʱʱ��Ľڵ㣬
        ��Ŀ�궨ʱ����������β��������������Ϊ�����µ�β�ڵ㡣*/
    if( !tmp ) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

// ��Ŀ�궨ʱ�� timer ��������ɾ��
void sort_timer_lst::del_timer( util_timer* timer )
{
    if( !timer ) {
        // http_conn::m_timer_lst_locker.unlock();
        return;
    }
    // �����������������ʾ������ֻ��һ����ʱ������Ŀ�궨ʱ��
    if( ( timer == head ) && ( timer == tail ) ) {
        delete timer;
        head = NULL;
        tail = NULL;
    }
    /* ���������������������ʱ������Ŀ�궨ʱ���������ͷ�ڵ㣬
        �������ͷ�ڵ�����Ϊԭͷ�ڵ����һ���ڵ㣬Ȼ��ɾ��Ŀ�궨ʱ���� */
    else if( timer == head ) {
        head = head->next;
        head->prev = NULL;
        delete timer;
    }
    /* ���������������������ʱ������Ŀ�궨ʱ���������β�ڵ㣬
    �������β�ڵ�����Ϊԭβ�ڵ��ǰһ���ڵ㣬Ȼ��ɾ��Ŀ�궨ʱ����*/
    else if( timer == tail ) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
    }
    // ���Ŀ�궨ʱ��λ��������м䣬�����ǰ��Ķ�ʱ������������Ȼ��ɾ��Ŀ�궨ʱ��
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }
}


/* SIGALARM �ź�ÿ�α������������źŴ�������ִ��һ�� tick() �������Դ��������ϵ�������*/
void sort_timer_lst::tick() {
    if( !head ) {
        return;
    }
    cout<<"timer tick"<<endl;
    time_t curr_time = time(NULL);  // ��ȡ��ǰϵͳʱ��
    util_timer* tmp = head;
    // ��ͷ�ڵ㿪ʼ���δ���ÿ����ʱ����ֱ������һ����δ���ڵĶ�ʱ��
    while( tmp ) {
        /* ��Ϊÿ����ʱ����ʹ�þ���ʱ����Ϊ��ʱֵ�����Կ��԰Ѷ�ʱ���ĳ�ʱֵ��ϵͳ��ǰʱ�䣬
        �Ƚ����ж϶�ʱ���Ƿ���*/
        if( curr_time < tmp->expire ) {   // ��ǰδ��ʱ�������ڵ�Ҳ����ʱ
            break;
        }

        // ���ö�ʱ���Ļص���������ִ�ж�ʱ���񣬹ر�����
        tmp->user_data->close_con();
        // ɾ����ʱ��
        del_timer(tmp);
        tmp = head;
    }
}
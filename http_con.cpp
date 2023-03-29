#include"http_con.h"

//��ʼ���ͻ�����������,��̬��Ա������Ҫ��ʼ��
int http_con::m_user_count = 0;
//��ʼ��epoll���ļ�������
int http_con::m_epollfd = -1;
sort_timer_lst http_con::m_timer_lst;

// ����HTTP��Ӧ��һЩ״̬��Ϣ
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

// ��վ�ĸ�Ŀ¼
const char* doc_root = "/home/xzb/code/resources";

//���캯��
http_con::http_con(){

}

//��������
http_con::~http_con(){

}

//�����ļ�������Ϊ��������ʽ
void setnonblock(int fd) {
    //��ȡ�ļ�������״̬
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

//��epoll�������Ҫ�������ļ�������
void addfd(int epollfd, int fd, bool one_shoot, bool et) {
    //�����ļ���������Ϣ
    struct epoll_event epev;
    epev.data.fd = fd;
    if (et) {
        epev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }else {
        epev.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shoot) {//��ֹͬһ��ͨ�ű���ͬ���̴߳���
        epev.events |= EPOLLONESHOT;
    }
    //��epollfd��Ӽ������ļ�������
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &epev);
    //����Ϊ��������ʽ
    setnonblock(fd);
}

//��epollfdɾ���ļ�������
void removefd(int epollfd, int fd) {
    //��epollfd��ɾ���ļ�������
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    //�ر��ļ�������
    close(fd);
}

//�޸�epollfd��fd��״̬
void modfd(int epollfd, int fd, int ev) {
    struct epoll_event epev;
    epev.data.fd = fd;
    epev.events = ev | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &epev);
}

//��ʼ���µĿͻ������ӣ�Ҳ���ǽ��ͻ���������ӵ�epoll��������
void http_con::init(int sockfd, const sockaddr_in& addr){
    m_sockfd = sockfd;
    m_address = addr;

    //���ö˿ڸ���
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, m_sockfd, true, ET);
    m_user_count++;
    init();

    // ������ʱ����������ص������볬ʱʱ�䣬Ȼ��󶨶�ʱ�����û����ݣ���󽫶�ʱ����ӵ�����timer_lst��
    util_timer* new_timer = new util_timer;
    new_timer->user_data = this;
    time_t curr_time = time(NULL);
    new_timer->expire = curr_time + 3 * TIMESLOT;
    this->timer = new_timer;
    m_timer_lst.add_timer(new_timer);  
}

void http_con::init() {

    bytes_to_send = 0;
    bytes_have_send = 0;

    m_read_index = 0;                 
    m_checked_index = 0;               
    m_start_line = 0;                  
    m_url = 0;                  
    m_version = 0;
    m_host = 0;
    m_content_length = 0;
    m_write_idx = 0;

    m_method = GET;
    m_linger = false;
    m_check_state = CHECK_STATE_REQUESTLINE; //��ʼ״̬Ϊ���������

    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
}

//�ر��ļ�������
void http_con::close_con(){
    if (m_sockfd != -1) {
        //��epollfd����������ɾ���ͻ��˵�fd�����ҹرտͻ��˵��ļ�������
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//һ���Զ������е�����
bool http_con::read(){
    if(timer) {//�����ݶ�ȡ��Ҫ���³�ʱ��ʱ��              
        time_t curr_time = time( NULL );
        timer->expire = curr_time + 3 * TIMESLOT;
        m_timer_lst.adjust_timer( timer );
    }
    // cout<<"read data!"<<endl;
    if (m_read_index >= READ_BUFFER_SIZE) {
        return false;
    }

    int read_ret = 0;
    while (true) {
        read_ret = recv(m_sockfd, m_read_buf + m_read_index,
        READ_BUFFER_SIZE - m_read_index, 0);
        if (read_ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;//û������
            }
            return false;
        }else if (read_ret == 0) {
            //�ر�����
            return false;
        }
        m_read_index += read_ret;
    }
    // cout<<m_read_buf<<endl;
    return true;
}

//�Լ������д�¼�
bool http_con::write(){
    int temp = 0;
    // int bytes_have_send = 0;    // �Ѿ����͵��ֽ�
    // int bytes_to_send = m_write_idx;// ��Ҫ���͵��ֽ� ��m_write_idx��д�������д����͵��ֽ���
    
    if(timer) {//д������Ҫ���³�ʱ��ʱ��              
        time_t curr_time = time( NULL );
        timer->expire = curr_time + 3 * TIMESLOT;
        m_timer_lst.adjust_timer( timer );
    }

    if ( bytes_to_send == 0 ) {
        // ��Ҫ���͵��ֽ�Ϊ0����һ����Ӧ������
        modfd( m_epollfd, m_sockfd, EPOLLIN ); 
        init();
        return true;
    }

    while(1) {
        // ��ɢд������д���ֽ���
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if ( temp <= -1 ) {
            // ���TCPд����û�пռ䣬��ȴ���һ��EPOLLOUT�¼�����Ȼ�ڴ��ڼ䣬
            // �������޷��������յ�ͬһ�ͻ�����һ�����󣬵����Ա�֤���ӵ������ԡ�
            if( errno == EAGAIN ) {
                modfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();
            return false;
        }
        //�ܷ����ֽ������Ѿ������ֽ�������
        bytes_to_send -= temp;
        bytes_have_send += temp;
        //�Ѿ������ֽ������ڵ���writebufʣ��Ҫ���͵��ֽ���
        if (bytes_have_send >= m_iv[0].iov_len){
            //��Ӧͷ�Ѿ��������
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else{
            //����writebuf
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }

        if (bytes_to_send <= 0){
            // û������Ҫ������
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if (m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
        }
    }
}

//����һ�����ݣ��ж��Ƿ���������\r\n�ж�
//��ʽ��GET / HTTP/1.1\r\n
//���һ����m_chekced_index   
http_con::LINE_STATUS http_con::parse_line() {
    //�������ַ�����
    char temp;
    //��m_checked_index��ǰ�������ַ��ڶ���������λ�ã����в�ѯ
    for (; m_checked_index < m_read_index; m_checked_index++) {
        temp = m_read_buf[m_checked_index];
        if (temp == '\r') {//Ѱ����һ���ַ�
            if((m_checked_index+1) == m_read_index) {
                //�в�������δ��ȡ��  
                return LINE_OPEN;
            }else if (m_read_buf[m_checked_index+1] == '\n') {
                //��������ĩβ����\r��\n����\0
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if (temp == '\n') {
            if ((m_checked_index > 1) && (m_read_buf[m_checked_index-1] == '\r')) {
                m_read_buf[m_checked_index-1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//http�����н���
http_con::HTTP_CODE http_con::parse_request_line(char* text) {
    //GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t");//��������и�ʽ�Ƿ���ȷ
    if (!m_url) {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char* method = text;
    if (strcasecmp(method, "GET") == 0) {//���Դ�Сд
        m_method = GET;
    }else {
        return BAD_REQUEST;
    }
    // /index.html HTTP/1.1
    //Ѱ�Ұ汾��
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return BAD_REQUEST;
    } 
    *m_version++ = '\0';
    if(strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    /**��һ�����
     * http://192.168.110.129:10000/index.html
    */
    if (strncasecmp(m_url, "http://", 7) == 0 ) {   
        m_url += 7;
        // �ڲ��� str ��ָ����ַ�����������һ�γ����ַ� c��һ���޷����ַ�����λ�á�
        m_url = strchr( m_url, '/' );
    }
    if ( !m_url || m_url[0] != '/' ) {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//http����ͷ������
http_con::HTTP_CODE http_con::parse_headers(char* text) {
    //�������У���ʾͷ���������
    if (text[0] == '\0') {
        //���HTTP��������Ϣ�壬 ����Ҫ��ȡm_content_length�ֽڵ���Ϣ
        //״̬ת�Ƶ�CHECK_STATE_CONTENT
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;//�������Ŀͻ�������
        }
        return GET_REQUEST;//������http����
    }else if (strncasecmp(text, "Connection:", 11) == 0) {
        //����Connectionͷ���ֶ� Connection��keep-alive
        text += 11;
        text += strspn(text, " \t");//ƥ�䵽keep-alive
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;//http���������������
        }
    }else if (strncasecmp( text, "Content-Length:", 15 ) == 0) {
        // ����Content-Lengthͷ���ֶΣ� ��ȡhttp������Ϣ�ĳ���
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }else if (strncasecmp( text, "Host:", 5 ) == 0) {
        //����hostͷ���ֶ�
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }else {
        cout<<"oop! unknow header "<<text<<endl;
    }
    return NO_REQUEST;
}

//http�������ݽ���
//����û����������HTTP�������Ϣ�壬ֻ���ж����Ƿ������Ķ�����
http_con::HTTP_CODE http_con::parse_content(char* text) {
    if (m_read_index >= (m_content_length + m_checked_index)) {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// ���õ�һ����������ȷ��HTTP����ʱ�����Ǿͷ���Ŀ���ļ������ԣ�
// ���Ŀ���ļ����ڡ��������û��ɶ����Ҳ���Ŀ¼����ʹ��mmap����
// ӳ�䵽�ڴ��ַm_file_address���������ߵ����߻�ȡ�ļ��ɹ�
http_con::HTTP_CODE http_con::do_request() {
    //"/home/xzb/code/resources"
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    //ƴ��Ŀ¼
    strncpy( m_real_file + len, "/index.html", FILENAME_LEN - len - 1 );
    // ��ȡm_real_file�ļ�����ص�״̬��Ϣ��-1ʧ�ܣ�0�ɹ�
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        return NO_RESOURCE;
    }

    // �жϷ���Ȩ��
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        return FORBIDDEN_REQUEST;
    }

    // �ж��Ƿ���Ŀ¼
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        return BAD_REQUEST;
    }

    // ��ֻ����ʽ���ļ�
    int fd = open( m_real_file, O_RDONLY );
    // �����ڴ�ӳ��
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return FILE_REQUEST;
}

// ���ڴ�ӳ����ִ��munmap����
void http_con::unmap() {
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}

//����http�����ģ������ĸ�ʽ�������С������ײ�����������
http_con::HTTP_CODE http_con::process_read() {
    //��ʼ���ж�ȡ��Ϣ-�����ж�ȡ
    LINE_STATUS line_status = LINE_OK;
    //��ʼ��http�������״̬
    HTTP_CODE ret = NO_REQUEST;
    //����һ�е�����
    char* text = 0;
    //�ж�������е������Ƿ�����,��״̬��Ϊ�������ݲ���Ҫһ��һ�н���
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
                || ((line_status = parse_line()) == LINE_OK)) {
        //��ȡһ������
        text = get_line();
        // cout<<"text: "<<text<<endl;
        //�����ݵ���ʼλ�õ��ڽ����ַ��ڶ��������е�λ��
        m_start_line = m_checked_index;
        cout<<"get 1 http line:"<<text<<endl;

        //״̬��ת��
        switch (m_check_state) {
            //�����н���
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                // cout<<"request ret: "<< ret <<endl;
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            //����ͷ������
            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                // cout<<"header ret: "<< ret <<endl;
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }else if (ret == GET_REQUEST) {
                    //http�������������н���
                    return do_request();
                }
                break;
            }
            //�������ݽ���
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                // cout<<"content ret: "<< ret <<endl;
                if (ret == GET_REQUEST) {
                    //http�������������н���
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            //����
            default: {
                //���ش���
                return INTERNAL_ERROR;
            }
        }
    }
    //��ʾ�������Ŀͻ�������
    return NO_REQUEST;
}

//http��Ӧ����
bool http_con::process_write(HTTP_CODE ret) {
    //����������Ӧ�����ķ���ֵ�ж�
    switch (ret){
        case INTERNAL_ERROR:
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) ) {
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        case FILE_REQUEST:
            add_status_line(200, ok_200_title );
            add_headers(m_file_stat.st_size);
            m_iv[ 0 ].iov_base = m_write_buf;
            m_iv[ 0 ].iov_len = m_write_idx;
            m_iv[ 1 ].iov_base = m_file_address;
            m_iv[ 1 ].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            //��Ҫ�������ݵ��ֽ���=��Ӧͷ��С+�ļ��Ĵ�С
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        default:
            return false;
    }
    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    //����1�����⣬һ��Ҫ����Ҫ���͵��ֽ�����Ϊд�������Ĵ�С
    bytes_to_send = m_write_idx;
    return true;
}

// ��д������д������͵�����
bool http_con::add_response( const char* format, ... ) {
    //���д���ݵ������������Χ���˳�
    if( m_write_idx >= WRITE_BUFFER_SIZE ) {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) ) {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

//��д������д����Ӧ������
bool http_con::add_status_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}
//��д������д����Ӧͷ����
bool http_con::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}
//��д������д����Ӧ���ݳ�������
bool http_con::add_content_length(int content_len) {
    return add_response( "Content-Length: %d\r\n", content_len );
}
//��д������д��connection��Ϣ
bool http_con::add_linger(){
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}
//��д������д��հ���
bool http_con::add_blank_line(){
    return add_response( "%s", "\r\n" );
}
//��д������д������
bool http_con::add_content( const char* content ){
    return add_response( "%s", content );
}
//��д������д��content type
bool http_con::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

//����������
void http_con::process(){
    
    //����http������
    HTTP_CODE read_ret = process_read();
    // cout<<"read_ret: "<<read_ret<<endl;
    if (read_ret == NO_REQUEST) {//������������Ҫ������ȡ�ͻ�����
        //��ֵ�ͻ��˵��ļ�������״̬
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return ;
    }

    //������Ӧ
    bool write_ret = process_write(read_ret);
    // cout<<"write_ret: "<<write_ret<<endl;
    if (!write_ret) {
        close_con();//�ر�����
        if(timer) m_timer_lst.del_timer(timer);
    }
    //����epollonrshot
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

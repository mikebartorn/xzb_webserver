#include"../include/http_con.h"

//初始化客户端连接数量,静态成员变量需要初始化
int http_con::m_user_count = 0;
//初始化epoll的文件描述符
int http_con::m_epollfd = -1;
sort_timer_lst http_con::m_timer_lst;

map<string, string> m_users;        //存放用户登录名和密码的map
locker m_lock; 

// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

// 网站的根目录
const char* doc_root = "/home/abin/code/xzb_webserver/resources";

//从数据库连接池中取出一个数据库，往map存放用户名和密码
void http_con::initmysql_result(Connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    ConnectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)){
        string temp1(row[0]);
        string temp2(row[1]);
        m_users[temp1] = temp2;
    }
}

//构造函数
http_con::http_con(){

}

//析构函数
http_con::~http_con(){

}

//设置文件描述符为非阻塞形式
void setnonblock(int fd) {
    //获取文件描述的状态
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

//往epoll中添加需要监听的文件描述符
void addfd(int epollfd, int fd, bool one_shoot, bool et) {
    //配置文件描述符信息
    struct epoll_event epev;
    epev.data.fd = fd;
    if (et) {
        epev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }else {
        epev.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shoot) {//防止同一个通信被不同的线程处理
        epev.events |= EPOLLONESHOT;
    }
    //往epollfd添加监听的文件描述符
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &epev);
    //设置为非阻塞形式
    setnonblock(fd);
}

//往epollfd删除文件描述符
void removefd(int epollfd, int fd) {
    //往epollfd中删除文件描述符
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    //关闭文件描述符
    close(fd);
}

//修改epollfd中fd的状态
void modfd(int epollfd, int fd, int ev) {
    struct epoll_event epev;
    epev.data.fd = fd;
    epev.events = ev | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &epev);
}

//初始化新的客户端连接，也就是将客户端连接添加到epoll检测队列中
void http_con::init(int sockfd, const sockaddr_in& addr, int close_log, 
                    string user, string passwd, string basename){
    m_sockfd = sockfd;
    m_address = addr;
    m_close_log = close_log;

    //设置端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, m_sockfd, true, ET);
    m_user_count++;
    init();

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, basename.c_str());

    // 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_lst中
    util_timer* new_timer = new util_timer;
    new_timer->user_data = this;
    time_t curr_time = time(NULL);
    new_timer->expire = curr_time + 3 * TIMESLOT;
    this->timer = new_timer;
    m_timer_lst.add_timer(new_timer);  
}

void http_con::init() {
    mysql = NULL;

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

    m_string = 0;

    m_method = GET;
    m_linger = false;
    m_check_state = CHECK_STATE_REQUESTLINE; //初始状态为检查请求行

    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
}

//关闭文件描述符
void http_con::close_con(){
    if (m_sockfd != -1) {
        //从epollfd监听队列中删除客户端的fd，并且关闭客户端的文件描述符
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//一次性读完所有的数据
bool http_con::read(){
    if(timer) {//有数据读取需要更新超时的时间              
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
                break;//没有数据
            }
            return false;
        }else if (read_ret == 0) {
            //关闭连接
            return false;
        }
        m_read_index += read_ret;
    }
    // cout<<m_read_buf<<endl;
    return true;
}

//解析一行数据，判断是否完整根据\r\n判断
//格式：GET / HTTP/1.1\r\n
//并且会更新m_chekced_index   
http_con::LINE_STATUS http_con::parse_line() {
    //分析的字符数据
    char temp;
    //以m_checked_index当前分析的字符在读缓冲区的位置，进行查询
    for (; m_checked_index < m_read_index; m_checked_index++) {
        temp = m_read_buf[m_checked_index];
        if (temp == '\r') {//寻找下一个字符
            if((m_checked_index+1) == m_read_index) {
                //行不完整还未读取完  
                return LINE_OPEN;
            }else if (m_read_buf[m_checked_index+1] == '\n') {
                //读到了行末尾，将\r和\n换成\0
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

//http请求行解析
http_con::HTTP_CODE http_con::parse_request_line(char* text) {
    //GET 报文：0/注册  1/登录
    //POST报文：2/登录校验  3/注册校验  5/图片  6/视频  7/关注
    m_url = strpbrk(text, " \t");//检查请求行格式是否正确
    if (!m_url) {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char* method = text;
    if (strcasecmp(method, "GET") == 0) {//忽略大小写
        m_method = GET;
    }else if (strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }else {
        return BAD_REQUEST;
    }
    // /index.html HTTP/1.1
    //寻找版本号
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return BAD_REQUEST;
    } 
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    if (strncasecmp(m_url, "http://", 7) == 0 ) {   
        m_url += 7;
        m_url = strchr( m_url, '/' );
    }
    if (strncasecmp(m_url, "https://", 8) == 0 ) {   
        m_url += 8;
        m_url = strchr( m_url, '/' );
    }
    if ( !m_url || m_url[0] != '/' ) {
        return BAD_REQUEST;
    }
    //url为‘/’时, 重新定义html文件
    if (strlen(m_url) == 1) {
        strcat(m_url, "judge.html");
    }
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//http请求头部解析
http_con::HTTP_CODE http_con::parse_headers(char* text) {
    //遇到空行，表示头部解析完毕
    if (text[0] == '\0') {
        //如果HTTP请求还有消息体， 还需要读取m_content_length字节的消息
        //状态转移到CHECK_STATE_CONTENT
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if (strncasecmp(text, "Connection:", 11) == 0) {
        //处理Connection头部字段 Connection：keep-alive
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    }else if (strncasecmp( text, "Content-Length:", 15 ) == 0) {
        // 处理Content-Length头部字段， 获取http请求消息的长度
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }else if (strncasecmp( text, "Host:", 5 ) == 0) {
        //处理host头部字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }else {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//http请求内容解析
//我们没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
http_con::HTTP_CODE http_con::parse_content(char* text) {
    if (m_read_index >= (m_content_length + m_checked_index)) {
        text[m_content_length] = '\0';
        //POST请求中最后的输入为用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
// 映射到内存地址m_file_address处，并告诉调用者获取文件成功
http_con::HTTP_CODE http_con::do_request() {
    //"/home/xzb/code/resources"
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    //找到/后的数据（post时）
    const char *p = strrchr(m_url, '/');

    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        //注册检验
        if (*(p + 1) == '3'){
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            if (m_users.find(name) == m_users.end()){
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                m_users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }else if (*(p+1) == '2') {//登录检验
            //判断登录名称密码是否对应
            if (m_users.find(name) != m_users.end() && m_users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    
    //注册 GET请求显示注册界面
    if (*(p+1) == '0') {

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if (*(p+1) == '1') {//登录 GET请求显示登录界面

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if (*(p+1) == '5') {

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if (*(p+1) == '6') {

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if (*(p+1) == '7') {

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else {
        //GET judeg.html
        strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
    }
    
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        return BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );
    // 创建内存映射
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return FILE_REQUEST;
}

// 对内存映射区执行munmap操作
void http_con::unmap() {
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}

//解析http请求报文，请求报文格式：请求行、请求首部、请求内容
http_con::HTTP_CODE http_con::process_read() {
    //初始化行读取信息-完整行读取
    LINE_STATUS line_status = LINE_OK;
    //初始化http请求解析状态
    HTTP_CODE ret = NO_REQUEST;
    //保存一行的数据
    char* text = 0;
    //判断请求的行的数据是否完整
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
                || ((line_status = parse_line()) == LINE_OK)) {
        //获取一行数据
        text = get_line();

        //行数据的起始位置等于解析字符在读缓冲区中的位置
        m_start_line = m_checked_index;
        LOG_INFO("%s", text);

        //状态机转换
        switch (m_check_state) {
            //请求行解析
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            //请求头部解析
            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }else if (ret == GET_REQUEST) {
                    //http请求完整，进行解析
                    return do_request();
                }
                break;
            }
            //请求内容解析
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if (ret == GET_REQUEST) {
                    //http请求完整，进行解析
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            //其他
            default: {
                //返回错误
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

//自己定义的写事件
bool http_con::write(){
    int temp = 0;
    if(timer) {//写数据需要更新超时的时间              
        time_t curr_time = time( NULL );
        timer->expire = curr_time + 3 * TIMESLOT;
        m_timer_lst.adjust_timer( timer );
    }

    if ( bytes_to_send == 0 ) {
        // 将要发送的字节为0，这一次响应结束。
        modfd( m_epollfd, m_sockfd, EPOLLIN ); 
        init();
        return true;
    }

    while(1) {
        // 分散写，返回写的字节数
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if ( temp <= -1 ) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if( errno == EAGAIN ) {
                modfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();
            return false;
        }
        //总发送字节数和已经发送字节数更新
        bytes_to_send -= temp;
        bytes_have_send += temp;
        //已经发送字节数大于等于writebuf剩余要发送的字节数
        if (bytes_have_send >= m_iv[0].iov_len){
            //响应头已经发送完毕
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else{
            //更新writebuf
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }

        if (bytes_to_send <= 0){
            // 没有数据要发送了
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

//http响应报文
bool http_con::process_write(HTTP_CODE ret) {
    //根据请求响应函数的返回值判断
    switch (ret){
        case INTERNAL_ERROR:{
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) ) {
                return false;
            }
            break;
        }
            
        case BAD_REQUEST:{
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        }
            
        case NO_RESOURCE:{
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:{
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        }   
        case FILE_REQUEST:{
            add_status_line(200, ok_200_title );
            if (m_file_stat.st_size != 0){
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }else{
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string))
                    return false;
            }
        }
            
        default:
            return false;
    }
    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    //这里1个问题，一定要将需要发送的字节数置为写缓冲区的大小
    bytes_to_send = m_write_idx;
    return true;
}

// 往写缓冲中写入待发送的数据
bool http_con::add_response( const char* format, ... ) {
    //如果写数据的索引大于最大范围，退出
    if( m_write_idx >= WRITE_BUFFER_SIZE ) {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) ) {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    LOG_INFO("request:%s", m_write_buf);
    return true;
}

//往写缓冲区写入响应行数据
bool http_con::add_status_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}
//往写缓冲区写入响应头数据
bool http_con::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}
//往写缓冲区写入响应内容长度数据
bool http_con::add_content_length(int content_len) {
    return add_response( "Content-Length: %d\r\n", content_len );
}
//往写缓冲区写入connection信息
bool http_con::add_linger(){
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}
//往写缓冲区写入空白行
bool http_con::add_blank_line(){
    return add_response( "%s", "\r\n" );
}
//往写缓冲区写入内容
bool http_con::add_content( const char* content ){
    return add_response( "%s", content );
}
//往写缓冲区写入content type
bool http_con::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

//工作处理函数
void http_con::process(){
    
    //解析http请求报文
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {//请求不完整，需要继续读取客户数据
        //充值客户端的文件描述符状态
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return ;
    }

    //生成响应
    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_con();//关闭连接
        if(timer) m_timer_lst.del_timer(timer);
    }
    //重置epollonrshot
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

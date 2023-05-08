#include "../include/webserver.h"

int main(int argc, char* argv[]) {
    string user = "root";
    string passwd = "xuzhubin943";
    string basename = "yourdb";
    Config config;
    config.parse_arg(argc, argv);
    
    Webserver server;

    server.init(config.port, config.close_log, user, passwd, basename, 
                config.max_conn);
    
    server.log_write();

    server.sql_init();

    server.thread_pool();

    server.eventlisten();

    server.eventloop();

    return 0;
}


#include "webserver.h"

int main(int argc, char* argv[]) {

    Config config;
    config.parse_arg(argc, argv);

    Webserver server;

    server.init(config.port, config.close_log);

    server.log_write();

    server.thread_pool();

    server.eventlisten();

    server.eventloop();

    return 0;
}


#include "webserver.h"

//Ö÷º¯Êý
int main(int argc, char* argv[]) {

    Config config;
    config.parse_arg(argc, argv);

    Webserver server;

    server.init(config.port);

    server.thread_pool();

    server.eventlisten();

    server.eventloop();

    return 0;
}


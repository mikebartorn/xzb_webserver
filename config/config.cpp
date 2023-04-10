#include "config.h"

Config::Config() {
    port = 10000;
    close_log = 0;
    max_conn = 8;
}

Config::~Config(){

}

void Config::parse_arg(int argc, char* argv[]) {
    int ret;
    const char *str = "p:s";
    while ((ret = getopt(argc, argv, str)) != -1) {
        switch (ret) {
            case 'p':{
                port = atoi(optarg);
                break;
            }
            case 's':{
                max_conn = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}

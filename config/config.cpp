#include "config.h"

Config::Config() {
    //?????10000
    port = 10000;
}

Config::~Config(){

}

void Config::parse_arg(int argc, char* argv[]) {
    int ret;
    const char *str = "p";
    while ((ret = getopt(argc, argv, str)) != -1) {
        switch (ret) {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                break;
        }
    }
}

#ifndef __SERVER_H__
#define __SERVER_H__

#include "utils.h"
#include <unordered_map>
#include <vector>
#include <string>

struct TCP_packet {

};

struct UDP_packet {
    char topic[50];
    char data_type;
    char content[1500];
};

struct client {
    char id[10];
    int sockfd;
};

#endif
#ifndef __SERVER_H__
#define __SERVER_H__

#include "utils.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <netinet/tcp.h>
#include <cstdio>
#include <sstream>

struct client {
    std::string id;
    int sockfd;
    int is_active;
    std::unordered_map<std::string, bool> subs;

    client() {};
    client(std::string id, int sockfd, int is_active):
        id(id), sockfd(sockfd), is_active(is_active) {}
};

#endif
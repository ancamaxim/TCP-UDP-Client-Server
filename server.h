#ifndef __SERVER_H__
#define __SERVER_H__

#include "utils.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <netinet/tcp.h>

struct client {
    string id;
    int sockfd;
    int is_active;

    client(string id, int sockfd, int is_active):
        id(id), sockfd(sockfd), is_active(is_active) {}
};

#endif
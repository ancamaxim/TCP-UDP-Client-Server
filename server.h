#ifndef __SERVER_H__
#define __SERVER_H__

#include "utils.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdio>
#include <sstream>

struct client {
    std::string id;
    int sockfd;
    int is_active;
    /* subscription topic and status : (de)activated */
    std::unordered_map<std::string, bool> subs;

    client() {};
    client(std::string id, int sockfd, int is_active):
        id(id), sockfd(sockfd), is_active(is_active) {}
};


/**
 * run_server = main logic for I/O interactions
 * 
 * @param listenTCP: TCP socket for listening for new connections
 * @param sock_UDP: UDP socket for receiving UDP notifications from client
 * 
 * @brief This function represents the running logic for the server's I/O
 * interactions, efficiently managed using poll. The server supports 4 types of
 * interactions: stdin input for exit command, accepting incoming connections
 * on the listening socket and handling client_id validity, receiving UDP
 * notifications from the corresponding client and receiving TCP subscription
 * messages from already connected clients.
 * 
 * Moreover, the server successfully handles however many connections through
 * dynamic reallocation mechanisms (both the poll connection buffer and the listening
 * queue are reallocated).
 * 
 */
void run_server(int listenTCP, int sock_UDP);

/**
 * parse_udp_pkt = parses input coming from UDP clients into a more readable format
 * 
 * @param udp_pkt:  UDP_packet object where the input will be stored after parsing
 *                  for better readability and further processing
 * @param buf: input from UDP client, which will be parsed
 * 
 * @brief Function parses low-level input from buf, respecting the struct's format:
 * topic + data_type + payload. Avoids writing into udp_pkt.payload more than necessary
 * (e.g.: when the payload is smaller than 1500 bytes)
 */
void parse_udp_pkt(UDP_packet *udp_pkt, char *buf);

/**
 * announce_clients =  sends TCP packets to clients that are subscribed to a topic
 *                     that matches the receiving UDP packet's topic
 * @param udp_pkt:  UDP packet, containing a notification to be further sent to
 *                  subscribed TCP clients
 * @param addr: IP address of UDP client
 * 
 * @brief Function iterates through database of clients, checks each client's subscription
 * and, if there's a match betweeen the UDP packet's topic and the subscription, it sends a
 * TCP packet to the client. Moreover, the function sends packets efficiently, splitting
 * one message into two separate parts: information about the UDP packet and the payload itself,
 * thus avoiding sending fixed size buffer (1500 bytes).
 */
void announce_clients(UDP_packet udp_pkt, sockaddr_in addr);

/**
 * handle_subscription =  mechanism for registering incoming (un)subscriptions from clients
 * 
 * @param tcp_sub: Flag for detecting (un)subscriptions + topic
 * @param sockfd: socket for client connection
 * 
 * @brief Function checks whether the TCP message from the client is a subscription, 
 * in which case it registers the topic in the client's subscription map, or an unsubscription,
 * in which case it erases the entry from the map and deactivates all matching subscriptions
 * (in case of a wildcard topic).
 */
void handle_subscription(TCP_subscription tcp_sub, int sockfd);

/**
 * split =  splits text into tokens based on delimiter
 * 
 * @param text: text to be split
 * @param delim: delimiter character
 * 
 * @brief Function returns a vector of tokens separated in text by a delimiter.
 */
vector<string> split(const string& text, char delim);

/**
 * match_topic =  checks whether string s matches pattern p
 * 
 * @param s: original text
 * @param delim: pattern
 * 
 * @brief Function returns true if s matches pattern p, with support for wildcard (*)
 * and single character (+) matching.
 */
bool match_topic(vector<string> s, vector<string> p);

/**
 * close_all_connections =  closes all active connection in pollfd array
 * 
 * @param poll_fds: File descriptor array with open connections
 * @param num_sockets: size of pollfd array
 * 
 * @brief Function closes all active connections, using close_client() function.
 */
void close_all_connections(pollfd *poll_fds, int num_sockets);

/**
 * close_client =  closes client connection
 * 
 * @param poll_fds: File descriptor array with open connections
 * @param i: index of client in pollfd array
 * 
 * @brief Function cleanly closes the client connection, while also deactivating
 * all of client's entries in the data structures used to keep track of the client's
 * information.
 */
void close_client(pollfd *poll_fds, int i);

#endif
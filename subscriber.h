#ifndef __SUBSCRIBER_H__
#define __SUBSCRIBER_H__

#include "utils.h"
#include <cmath>

/**
 * run_client = main logic for client-server interaction
 * 
 * @param sockfd: TCP connection socket
 * 
 * @brief This function represents the running logic for the client's interactions
 * with the server and stdin, succesfully handling (un)subscribe requests from
 * the user. The client can also receive notifications from the server. This
 * type of messages are printed based on a standard format.
 * 
 * Moreover, the function handles the situation where the server has closed the
 * communication and exits.
 * 
 */
void run_client(int sockfd);

/**
 * print_notification = prints TCP message content based on format
 * 
 * @param tcp_notif: TCP message information
 * @param ip_udp: IP of UDP client where the notification was sent from
 * @param payload: payload of UDP notification
 * 
 * @brief Function prints message according to a certain format. This function
 * is also responsible for doing the dirty work of payload parsing, based on 
 * data type.
 * 
 */
void print_notification(TCP_notification tcp_notif, char *ip_udp, char *payload);

/**
 * parse_tcp = user input parsing in TCP_subscription struct format
 * 
 * @param tcp_pkt: future TCP message filled with data from buf
 * @param buf: user input
 * 
 * @brief Function parses user input ((un)subscription messages), based on 
 * TCP_subscription struct format. This format is used for better readability and
 * scalability.
 * 
 */
void parse_tcp(TCP_subscription *tcp_pkt, char *buf);

#endif

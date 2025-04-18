#ifndef __UTILS_H__
#define __UTILS_H__

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define    MAX_TCP_MESSAGE   1500

/**
 * DIE = Custom macro for error-checking
 * 
 * @param assertion: Predicate to be checked
 * @param call_description: Short description of error-causing function call
 * 
 * @brief This macro is used for signaling errors in libc function calls, 
 * which result in system call failure. A short description of the error will be
 * written to stderr. This macro causes the program to exit upon error.
 */

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

/**
 * send_all = sends len bytes from buff
 * 
 * @param sockfd: Socket file descriptor which the message will be send to
 * @param buff: Message buffer
 * @param len: Number of bytes to be send from buff
 * 
 * @brief The function is used to send exactly len bytes from buff.
 * It performs busy waiting, until all the specified bytes are sent.
 */
int send_all(int sockfd, void *buff, size_t len);

/**
 * recv_all = sends len bytes from buff
 * 
 * @param sockfd: Socket file descriptor which the message will be received from
 * @param buff: Message buffer
 * @param len: Number of bytes to be received from buff
 * 
 * @brief The function is used to receive exactly len bytes from buff.
 * It performs busy waiting, until all the specified bytes are received.
 */
int recv_all(int sockfd, void *buff, size_t len);

#endif

#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <istream>
#include <unistd.h>
#include <queue>

int set_socket_non_blocking(int sockfd);

/* read socket to msgbuf and process messages */
ssize_t sockfd_in(int sock, std::queue<std::string> &in_queue);

/* read socket to msgbuf and process messages */
int sockfd_out(int sock, std::queue<std::string> &out_queue);

#endif /* HELPERS_HPP */

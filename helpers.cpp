#include "helpers.hpp"
#include "data.hpp"
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sstream>
#include <errno.h>

#define BUFLEN 2056 // max size of message

int set_socket_non_blocking(int sockfd)
{
	int flags, s;

	flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl (sockfd, F_SETFL, flags);
	if (s == -1) {
		perror ("fcntl");
		return -1;
	}

	return 0;
}

ssize_t sockfd_in(int sock, std::queue<std::string> &in_queue)
{
	bool partial = false;
	if (!in_queue.empty()) {
		auto &back = in_queue.back();
		if (back.back() != '\n') {
			partial = true;
		}
	}

	char msgbuf[BUFLEN];
	ssize_t bytes_read = 1;
	while (bytes_read > 0) {
		bytes_read = recv(sock, &msgbuf, BUFLEN - 1, 0);
		if (bytes_read > 0) {
			msgbuf[bytes_read] = '\0';
			std::stringstream in;
			in << msgbuf;
			std::string msg;
			while (getline(in, msg, '\n')) {
				if (msg.size() > 0) {
					if (partial) {
						in_queue.back() += msg + "\n";
					} else {
						in_queue.push(msg + "\n");
					}
				}
			}
		}
	}
	return bytes_read;
}

int sockfd_out(int sock, std::queue<std::string> &out_queue)
{	
	while (!out_queue.empty()) {
		auto &msg = out_queue.front();
		ssize_t bytes_written = send(sock, msg.c_str(), msg.size(), 0);
		int err = errno;
		
		if (bytes_written < 1) {
			return err;
		} else if (static_cast<unsigned long>(bytes_written) < msg.size()) {
			out_queue.front() = msg.substr(bytes_written - 1, msg.size());
		} else {
			out_queue.pop();
		}
	}
	return 0;
}

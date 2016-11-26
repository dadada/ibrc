#include "helpers.hpp"
#include <fcntl.h>
#include <stdio.h>

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


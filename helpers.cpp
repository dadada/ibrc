#include "helpers.hpp"
#include "data.hpp"
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sstream>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <cstring>

int set_socket_non_blocking(int sockfd)
{
	return set_socket_opt(sockfd, O_NONBLOCK);
}

int set_socket_blocking(int sockfd)
{
	return unset_socket_opt(sockfd, O_NONBLOCK);
}

int set_socket_opt(int sockfd, int opt)
{
	int flags, s;

	flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		return -1;
	}

	flags |= opt;
	s = fcntl (sockfd, F_SETFL, flags);
	if (s == -1) {
		perror ("fcntl");
		return -1;
	}

	return 0;
}

int unset_socket_opt(int sockfd, int opt)
{
	int flags, s;

	flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		return -1;
	}

	flags &= ~opt;
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
					partial = in.eof();
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
		//TODO remove
		std::cout << "sending: " << msg;
		//
		ssize_t bytes_written = send(sock, msg.c_str(), msg.size(), 0);
		int err = errno;
		
		if (bytes_written < 1) {
			return err;
		} else if (static_cast<unsigned long>(bytes_written) < msg.size()) {
			out_queue.front() = msg.substr(bytes_written, msg.size());
		} else {
			out_queue.pop();
		}
	}
	return 0;
}

connection_manager::connection_manager()
{
	epollfd = epoll_create1(0);

	events = new struct epoll_event[MAX_EVENTS];

	if (epollfd == -1) {
		perror("epoll_create1");
	}
}

connection_manager::~connection_manager()
{
	delete[] events;
	for (auto s : socks) {
		if (close(s) == -1) {
			perror("close");
		}
	}
}

int connection_manager::wait_events()
{
	next_event_pos = 0;

	count_events = epoll_wait(epollfd, events, MAX_EVENTS, -1);

	if (count_events == -1) {
		perror("epoll_wait");
		return -1;
	}

	return count_events;
}

bool connection_manager::next_event(struct epoll_event &ev)
{
	if (next_event_pos < count_events && next_event_pos < MAX_EVENTS) {
		ev = events[next_event_pos];
		next_event_pos++;
		if (socks.find(ev.data.fd) == socks.end()) {
			return false;
		}
		return true;
	}
	return false;
}

int connection_manager::add_accepting(std::string port)
{
	struct addrinfo *ainfo;
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = 0;
	if ((status = getaddrinfo(NULL, port.c_str(), &hints, &ainfo)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return -1;
	}

	int listen_s = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (listen_s == -1) {
		perror("socket");
		return -1;
	}

	int yes = 1;
	if (setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		remove_socket(listen_s);
		return -1;
	}

	if (bind(listen_s, ainfo->ai_addr, ainfo->ai_addrlen) != 0) {
		perror("bind");
		remove_socket(listen_s);
		return -1;
	}

	if (listen(listen_s, 5) != 0) {
		perror("listen");
		remove_socket(listen_s);
		return -1;
	}

	if (add_socket(listen_s, EPOLLFLAGS) < 0) {
		return -1;
	}

	return listen_s;

}

int connection_manager::add_socket(int sockfd, int flags)
{
	if (set_socket_non_blocking(sockfd) != 0) {
		return -1;
	}
	
	struct epoll_event ev;

	ev.events = flags;
	ev.data.fd = sockfd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) != 0) {
		remove_socket(sockfd);
		perror("epoll_ctl: add sockfd failed.");
		return -1;
	}

	socks.insert(sockfd);

	return sockfd;
}

int connection_manager::accept_client(int sock)
{

	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof addr;

	int conn_s = accept(sock, (struct sockaddr *) &addr, &addrlen);
	if (conn_s == -1) {
		perror("accept");
		return -1;
	}

	if (set_socket_non_blocking(conn_s) == -1) {
		remove_socket(conn_s);
		return -1;
	}

	if (add_socket(conn_s, EPOLLFLAGS | EPOLLOUT) < 0) {
		return -1;
	}

	return conn_s;
}

bool connection_manager::remove_socket(int sock)
{
	in_queues.erase(sock);
	out_queues.erase(sock);
	socks.erase(sock);
	if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, nullptr) != 0) {
		perror("epoll_ctl: mod sockfd failed.");
		return false;
	} else if (close(sock) == -1) {
		perror("close");
		return false;
	}
	return true;
}

bool connection_manager::pause_write(int sock)
{
	return epoll_mod(sock, EPOLLFLAGS);
}

bool connection_manager::continue_write(int sock)
{
	return epoll_mod(sock, EPOLLFLAGS | EPOLLOUT);
}

bool connection_manager::epoll_mod(int sock, uint32_t event_flags)
{
	struct epoll_event ev;
	ev.data.fd = sock;
	ev.events = event_flags;

	if (epoll_ctl(epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev) != 0) {
		perror("epoll_ctl: mod sockfd failed.");
		return false;
	}
	return true;
}

bool connection_manager::add_message(int sock, std::string message)
{
	out_queues[sock].push(message);
	return continue_write(sock);
}

bool connection_manager::fetch_message(int sock, std::string &msg)
{
	if (!in_queues[sock].empty()) {
		msg = in_queues[sock].front();
		if (msg.size() > 0 && msg.back() == '\n') {
			in_queues[sock].pop();
			return true;
		}
	}
	return false;
}

bool connection_manager::receive_messages(int sock)
{
	auto count = sockfd_in(sock, in_queues[sock]);
	if (count == 0) {
		remove_socket(sock);
	}
	return count != 0;
}

bool connection_manager::send_messages(int sock)
{
	int err = sockfd_out(sock, out_queues[sock]);

	if (err == 0 && out_queues[sock].empty()) {
		pause_write(sock);
		return true;
	} else if (err == EAGAIN || err == EWOULDBLOCK) {
		continue_write(sock);
		return true;
	} else {
		remove_socket(sock);
		return false;
	}
}

int connection_manager::create_connection(std::string host, std::string port)
{
	struct addrinfo *ainfo;
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	int status = 0;
	if ((status = getaddrinfo(host.c_str(), port.c_str(), &hints, &ainfo)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return -1;
	}

	int sock = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (sock == -1) {
		perror("socket");
		return -1;
	}

	int yes = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		remove_socket(sock);
		return -1;
	}

	if (connect(sock, ainfo->ai_addr, ainfo->ai_addrlen) == -1) {
		perror("connect");
		remove_socket(sock);
		return -1;
	}

	if (set_socket_non_blocking(sock) != 0) {
		remove_socket(sock);
		return -1;
	}

	struct epoll_event ev;

	ev.events = EPOLLFLAGS;
	ev.data.fd = sock;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) != 0) {
		perror("epoll_ctl: add sockfd failed.");
		remove_socket(sock);
		return -1;
	}

	socks.insert(sock);

	return sock;
}

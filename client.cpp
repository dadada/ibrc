#include "client.hpp"
#include "helpers.hpp"
#include <iostream>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <sstream>
#include <sys/epoll.h>

#define USAGE "usage: ibrcc <hostname> <port>"

int main(int argc, char* argv[])
{
	std::string port = DEFAULT_SERVER_PORT;
	std::string host;
	if (argc < 2) {
		std::cerr << "ibrcc: too few arguments provided" << USAGE << std::endl;
		exit(EXIT_FAILURE);
	} else if (argc > 3) {
		std::cerr << "ibrcc: excess arguments" << std::endl << USAGE << std::endl;
	} else {
		host = argv[1];
		if (argc > 2) {
			port = argv[2];
		}
	}

	try {
		client c = client(host, port);
		if (c.run() != 0) {
			std::cerr << "an unknown error occured" << std::endl;
		}
	} catch (client_exception& e) {
		std::cerr << e.what() << std::endl;
	}

	exit(EXIT_SUCCESS);
}

client::client(std::string host, std::string client_port)
{
	sockfd = 0; // stdin
	current_channel = ""; // empty channel is no channel
	if (connect_client(host, client_port) != 0) {
		throw client_exception();
	}
}

client::~client()
{
	quit();
}

const char* client_exception::what() const throw()
{
	return "client: failed to create a client";
}

int client::connect_client(std::string host, std::string client_port)
{
	if (sockfd != 0) {
		return -1;
	}

	struct addrinfo *ainfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	int status;
	if ((status = getaddrinfo(host.c_str(), client_port.c_str(), &hints, &ainfo)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
		return -1;
	}

	sockfd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (sockfd == -1) {
		perror("socket");
		return -1;
	}

	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		return -1;
	}
	
	if (connect(sockfd, ainfo->ai_addr, ainfo->ai_addrlen) == -1) {
		perror("connect");
		return -1;
	}

	char hostn[1024];
	hostn[1023] = '\0';
	gethostname(hostn, 1023);
	hostname = std::string(hostn);

	socklen_t len;
	struct sockaddr_storage addr;

	if (getpeername(sockfd, (struct sockaddr*)&addr, &len) != 0) {
		perror("getpeername");
		return -1;
	}
	
	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		port = ntohs(s->sin_port);
	} else {
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		port = ntohs(s->sin6_port);
	}
	
	if (send_message("CONNECT", "") != 0) {
		return -1;
	}

	return 0;
}

int client::send_message(std::string msg_name, std::string msg_payload)
{
	if (!connected()) {
		return -1;
	}

	std::stringstream msg;

	msg << msg_name << " " << hostname << " " << port << " " << msg_payload;

	const std::string& msg_str = msg.str();
	const char* sendbuf = msg_str.c_str();
	if (send(sockfd, sendbuf, strlen(sendbuf), 0) < 0) {
		perror("send");
		return -1;
	}

	return 0;
}

std::string client::receive_message()
{
	char buf[2056];
	ssize_t byte_count;

	// one byte for string terminator
        byte_count= recv(sockfd, buf, 2055, 0);
	if (byte_count == -1) {
		perror("recv");
		throw receive_error();
	} else if (byte_count == 0) {
		disconnect();
	}

	// terminate the string byte_count < 2056 ?
	buf[byte_count] = '\0';
	std::stringstream input;
	input << buf;

	return input.str();
}

int client::disconnect()
{
	if (sockfd != 0 && send_message("DISCONNECT", "") != 0) {
		return -1;
	}

	close(sockfd);

	return 0;
}

int client::set_nick(std::string new_nick)
{
	if (send_message("NICK", new_nick) != 0) {
		return -1;
	} else {
		nick = new_nick;
	}

	return 0;
}

int client::join_channel(std::string chan_name)
{
	if (current_channel == chan_name) {
		return -1;
	}

	int status = send_message("JOIN", chan_name);
	if (status != 0) {
		return -1;
	}

	return 0;
}

int client::leave_channel(std::string chan_name)
{
	if (current_channel != "" && send_message("LEAVE", chan_name) != 0) {
		return -1;
	} else {
		current_channel = "";
	}

	return 0;
}

bool client::connected()
{
	return sockfd != 0;
}

int client::get_topic(std::string chan_name)
{
	if (send_message("GETTOPIC", chan_name) != 0) {
		return -1;
	}

	return 0;
}

int client::set_topic(std::string chan_name, std::string new_channel_topic)
{
	if (send_message("SETTOPIC", chan_name + new_channel_topic) != 0) {
		return -1;
	}

	return 0;
}

int client::send_channel_message(std::string channel_name, std::string message)
{
	if (send_message("MSG", channel_name + " " + message) != 0) {
		return -1;
	}

	return 0;
}

int client::send_private_message(std::string recipient, std::string channel, std::string message)
{
	if (send_message("PRIVMSG", recipient + " " + channel + " " + message) != 0) {
		return -1;
	}

	return 0;
}

int client::quit()
{
	return leave_channel(current_channel) && disconnect() ? 0 : -1;
}

int client::run()
{
	#define MAX_EVENTS 10

	struct epoll_event ev, events[MAX_EVENTS];

	int epollfd = epoll_create1(0);
	int user_fd = STDIN_FILENO;

	if (epollfd == -1 || (set_socket_non_blocking(sockfd) != 0)
	  || (set_socket_non_blocking(user_fd) != 0)) {
		return -1;
	}

	if (epollfd == -1) {
		perror("epoll_create1");
		return -1;
	}

	ev.events = EPOLLIN;
	ev.data.fd = user_fd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, user_fd, &ev) == -1) {
		perror("epoll_ctl: add user input fd failed.");
		return -1;
	}

	ev.events = EPOLLIN;
	ev.data.fd = sockfd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) != 0) {
		perror("epoll_ctr: add sockfd failed.");
		return 1;
	}

	while (true) {
		// no timeout
		int next_fd = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		if (next_fd == -1) {
			perror("epoll_wait");
			return -1;
		}

		for (int n = 0; n < next_fd; ++n) {
			if (events[n].data.fd == user_fd) {
				std::string command;
				std::getline(std::cin, command);
				process_command(command);
			} else if (events[n].data.fd == sockfd) {
				std::string msg = receive_message();
				process_message(msg);
			}
		}
	}

	return 0;
}

bool client::process_command(std::string command)
{
	std::istringstream cmd_stream(command);

	msg_type cmd;
	if (cmd_stream >> cmd) {
		std::string par1;
		std::string par2;
		switch (cmd) {
			case CONNECT:
				if (cmd_stream >> par1 >> par2) {
					connect_client(par1, par2);
				} else {
					std::cerr << "usage: CONNECT <host> <port>" << std::endl;
				}
				break;
			case DISCONNECT:
				disconnect();
				break;
			case NICK:
				if (cmd_stream >> par1) {
					set_nick(par1);
				} else {
					std::cerr << "usage: NICK <new nick>" << std::endl;
				}
				break;
			case JOIN:
				if (cmd_stream >> par1) {
					join_channel(par1);
				} else {
					std::cerr << "usage: JOIN <channel>" << std::endl;
				}
				break;
			case LEAVE:
				leave_channel(current_channel);
				break;
			case GETTOPIC:
				get_topic(current_channel);
				break;
			case SETTOPIC:
				if (cmd_stream >> par1) {
					set_topic(current_channel, par1);
				} else {
					std::cerr << "usage: SETTOPIC <topic>" << std::endl;
				}
				break;
			case MSG:
				if (std::getline(cmd_stream, par1, '\n')) {
					send_channel_message(current_channel, par1);
				} else {
					std::cerr << "usage: MSG <message>" << std::endl;
				}
				break;
			case PRIVMSG:
				if (cmd_stream >> par1 && std::getline(cmd_stream, par2, '\n')) {
					send_private_message(par1, current_channel, par2);
				} else {
					std::cerr << "usage: PRIVMSG <recipient> <message>" << std::endl;
				}
				break;
			case QUIT:
				return false;
			case HELP:
				std::cout << "commands: ";
				for (auto &c : command_names) {
					std::cout << c.second << ", ";
				}
				std::cout << HELP << std::endl;
				break;
			default:
				break;
		}
	} else {
		std::cerr << "not a valid command. see HELP for a list of commands" << std::endl;
	}
	return true;
}


void client::process_message(std::string msg)
{
	std::istringstream msg_stream(msg);
	msg_type cmd;
	std::string dest_host;
	int dest_port;
	status_code status;
	std::string topic;
	if (msg_stream >> cmd >> dest_host >> dest_port 
	  && dest_host == hostname && dest_port == port) {
		switch (cmd) {
			case STATUS:
				if (msg_stream >> status) {
					std::cout << "status: " << status << std::endl;
				}
				break;
			case NICKRES:
				if (msg_stream >> nick) {
					std::cout << "nick: you are now known as " << nick << std::endl;
				}
				break;
			case TOPIC:
				if (msg_stream >> topic) {
					std::cout << "topic: " << topic << std::endl;
				}
				break;
			default:
				break;
		}
	}
}

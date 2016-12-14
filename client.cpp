#include "client.hpp"
#include "helpers.hpp"
#include "data.hpp"
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
#include <signal.h>

#define USAGE "usage: ibrcc <hostname>"

int main(int argc, char* argv[])
{
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = client::cleanup;
	sigaction(SIGTERM, &action, NULL);

	std::string port = DEFAULT_PORT;
	std::string host;
	if (argc < 2) {
		std::cerr << "ibrcc: too few arguments provided" << USAGE << std::endl;
		exit(EXIT_FAILURE);
	} else if (argc > 2) {
		std::cerr << "ibrcc: excess arguments" << std::endl << USAGE << std::endl;
		exit(EXIT_FAILURE);
	} else {
		host = argv[1];
	}

	the_client = new client();

	if (the_client->connect_client(host, port) == 0) {
		if (!(the_client->run())) {
			std::cerr << "oopsi" << std::endl;
		}
	} else {
		std::cerr << "Failed to connect." << std::endl;
	}

	delete the_client;
	exit(EXIT_SUCCESS);
}

void client::cleanup(int signum)
{
	if (the_client != nullptr) {
		the_client->quit();
	}
}

client::client()
{
	quit_bit = false;
	sockfd = -1; // stdin
	current_channel = ""; // empty channel is no channel
}

const char* client_exception::what() const throw()
{
	return "client: failed to create a client";
}

int client::connect_client(std::string host, std::string server_port)
{
	struct addrinfo *ainfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	int status;
	if ((status = getaddrinfo(host.c_str(), server_port.c_str(), &hints, &ainfo)) != 0) {
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

	struct sockaddr_storage addr;
	socklen_t len = sizeof (addr);

	if (getsockname(sockfd, (struct sockaddr*)&addr, &len) != 0) {
		perror("getsockname");
		return -1;
	}
	
	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		port = ntohs(s->sin_port);
	} else {
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		port = ntohs(s->sin6_port);
	}

	struct epoll_event ev;

	epollfd = epoll_create1(0);

	if (epollfd == -1 || (set_socket_non_blocking(sockfd) != 0)
	  || (set_socket_non_blocking(STDIN_FILENO) != 0)) {
		return -1;
	}

	if (epollfd == -1) {
		perror("epoll_create1");
		return -1;
	}

	ev.events = EPOLLIN;
	ev.data.fd = STDIN_FILENO;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
		perror("epoll_ctl: add user input fd failed.");
		return -1;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = sockfd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) != 0) {
		perror("epoll_ctr: add sockfd failed.");
		return -1;
	}

	send_message("CONNECT", "");
	
	return 0;
}

int client::send_message(std::string msg_name, std::string msg_payload)
{
	if (!connected()) {
		return -1;
	}

	std::stringstream msg;

	msg << msg_name << " " << hostname << " " << msg_payload << std::endl;

	const std::string msg_str = msg.str();
	net_output.push(msg_str);

	struct epoll_event ev;

	ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
	ev.data.fd = sockfd;

	if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) != 0) {
		perror("epoll_ctl: mod sockfd failed.");
		return -1;
	}

	return 0;
}

int client::set_nick(std::string new_nick)
{
	if (nick == "") {
		nick = new_nick;
	}
	if (send_message("NICK", new_nick) != 0) {
		return -1;
	}
	nick = new_nick;

	return 0;
}

int client::join_channel(std::string chan_name)
{
	send_message("JOIN", chan_name);
	return 0;
}

int client::leave_channel(std::string chan_name)
{
	if (send_message("LEAVE", chan_name) != 0) {
		return -1;
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
	if (send_message("SETTOPIC", chan_name + " " + new_channel_topic) != 0) {
		return -1;
	}

	return 0;
}

int client::send_channel_message(std::string channel_name, std::string message)
{
	if (send_message("MSG", nick + " " + channel_name + " " + message) != 0) {
		return -1;
	}

	return 0;
}

int client::send_private_message(std::string recipient, std::string channel, std::string message)
{
	if (send_message("PRIVMSG", nick + " " + channel + " " + recipient + " " + message) != 0) {
		return -1;
	}

	return 0;
}

int client::quit()
{
	quit_bit = true;
	send_message("QUIT", "");
	return 0;
}

int client::run()
{
	struct epoll_event events[MAX_EVENTS];

	while (true) {
		// no timeout
		int count = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		if (count == -1) {
			perror("epoll_wait");
			return -1;
		}

		for (int n = 0; n < count; ++n) {
			struct epoll_event ev = events[n];
			if (ev.events & EPOLLERR || ev.events & EPOLLHUP) {
				close(ev.data.fd);
				quit_bit = true;
			} else if (ev.data.fd == STDIN_FILENO) {
				std::string cmd;
				std::getline(std::cin, cmd);
				if (process_command(cmd) == false) {
					std::cerr << "invalid command. see HELP" << std::endl;
				}
			} else if (ev.data.fd == sockfd) {

				if (ev.events & EPOLLIN) {
					if (sockfd_in(ev.data.fd, net_input) == 0) {
						quit();
					}
					consume_net_input();
				}
				if (ev.events & EPOLLOUT) {
					int status = sockfd_out(ev.data.fd, net_output);
					if (status == 0) {
						// disable waiting for socket if queue is empty
						ev.events = EPOLLIN | EPOLLET;
						if (epoll_ctl(epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev) != 0) {
							perror("epoll_ctr: mod sockfd failed.");
							return -1;
						}
						if (quit_bit) {
							return true;
						}
					}
				}
			}
		}
	}
	return 0;
}

void client::consume_net_input()
{
	std::stringstream ss;
	while (!net_input.empty()) {
		std::string &next = net_input.front();
		if (next.size() > 0) {
			if (next.back() == '\n') {
				process_message(next);
				net_input.pop();
			} else {
				break; // end reached
			}
		} else { // skip empty if exists
			net_input.pop();
		}
	}
}

bool client::process_command(std::string &command)
{
	std::istringstream cmd_stream(command);
	std::string cmd;
	std::string par1;
	std::string par2;
	if (cmd_stream >> cmd && cmd.front() == '/') {
		cmd = cmd.substr(1, cmd.size());
		std::stringstream cmdconverter (cmd);
		msg_type mtype;
		cmdconverter >> mtype;
		switch (mtype) {
			case NICK:
				if (cmd_stream >> par1) {
					return (set_nick(par1) == 0);
				} else {
					std::cerr << "usage: /NICK <new nick>" << std::endl;
				}
				break;
			case JOIN:
				if (cmd_stream >> par1) {
					return (join_channel(par1) == 0);
				} else {
					std::cerr << "usage: /JOIN <channel>" << std::endl;
				}
				break;
			case LEAVE:
				if (current_channel == "") {
					std::cerr << "must be joined to channel";
					return false;
				} else {
					return (leave_channel(current_channel) == 0);
				}
				break;
			case GETTOPIC:
				if (current_channel == "") {
					std::cerr << "must be joined to channel";
					return false;
				} else {
					return (get_topic(current_channel) == 0);
				}
				break;
			case SETTOPIC:
				if (current_channel == "") {
					std::cerr << "must be joined to channel";
					return false;
				} else if (getline(cmd_stream, par1, '\n')) {
					return (set_topic(current_channel, par1.substr(1,par1.size())) == 0);
				} else {
					std::cerr << "usage: /SETTOPIC <topic>" << std::endl;
				}
				break;
			case MSG:
				if (std::getline(cmd_stream, par1, '\n')) {
					return (send_channel_message(current_channel, par1.substr(1,par1.size())) == 0);
				} else {
					std::cerr << "usage: [/MSG] <message>" << std::endl;
				}
				break;
			case PRIVMSG:
				if (cmd_stream >> par1 && std::getline(cmd_stream, par2, '\n')) {
					return (send_private_message(par1, current_channel, par2.substr(1,par2.size())) == 0);
				} else {
					std::cerr << "usage: /PRIVMSG <recipient> <message>" << std::endl;
				}
				break;
			case LIST:
				return (send_list() == 0);
				break;
			case QUIT:
				return (quit() == 0);
				break;
			case HELP:
				std::cerr << "commands: ";
				for (auto &c : command_names) {
					std::cout << c << ", ";
				}
				std::cerr << HELP << std::endl;
				return true;
				break;
			default:
				return false;
				break;
		}
	} else {
		if (current_channel != "") {
			std::getline(cmd_stream, par1, '\n');
			return (send_channel_message(current_channel, cmd + par1) == 0);
		}
	}

	return false;
}

void client::process_message(std::string& msg)
{
	// TODO remove
	std::cout << "receiving: " << msg;
	//
	std::istringstream msg_stream(msg);
	msg_type cmd;
	std::string par1, par2, par3, par4;
	status_code status;
	if (msg_stream >> cmd >> par1) {
		if (hostname == par1) {
			switch (cmd) {
				case NICKRES:
					msg_stream >> nick;
					break;
				case STATUS:
					if (msg_stream >> status) {
						std::cout << "status: " << status << std::endl;
					}
					break;
				case LISTRES:
					std::cout << "listres: ";
					while (msg_stream >> par2) {
						std::cout << par2 << ", ";
					}
					std::cout << std::endl;
					break;
				case TOPIC:
					if (msg_stream >> par2 && std::getline(msg_stream, par3, '\n')) {
						std::cout << "topic for " << par2 << " is " << par3 << std::endl;
					}
					break;

				case CHANNEL:
					if (msg_stream >> par2 >> par3 && std::getline(msg_stream, par4, '\n')) {
						std::cout << "channel: you are joined to " << par2
							<< " as " << (hostname == par3 ? "OP" : "user")
							<< ", the topic is " << par4.substr(1, par4.size()) << std::endl;
						current_channel = par2;
					}
				default:
					break;
				}
		} else {
			std::string par5;
			switch (cmd) {
				case PRIVMSG:
					if (msg_stream >> par2 >> par3 >> par4 && std::getline(msg_stream, par5, '\n') && par3 == current_channel && par4 == nick) {
						std::cout << par2 << " whispers: " << par5.substr(1,par5.size()) << std::endl;
					}
					break;
				case MSG:
					if (msg_stream >> par2 >> par3 && std::getline(msg_stream, par4, '\n') 
							&& par3 == current_channel) {
						std::cout << par2 << ": " << par4.substr(1, par4.size()) << std::endl;
					}
					break;
				case DELCHANNEL:
					current_channel = "";
					break;
				default:
					break;
			}
		}
	}
}

int client::send_list()
{
	if (send_message("LIST", "") != 0) {
		return -1;
	}
	return 0;
}

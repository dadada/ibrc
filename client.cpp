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

	if (the_client->connect_client(host, port)) {
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
	sockfd = -1; // not 0, that would be stdout
	current_channel = ""; // empty channel is no channel

	char hostn[1024];
	hostn[1023] = '\0';
	gethostname(hostn, 1023);
	hostname = std::string(hostn);

	conman = new connection_manager();
	conman->add_socket(STDIN_FILENO, EPOLLFLAGS);
}

const char* client_exception::what() const throw()
{
	return "client: failed to create a client";
}

bool client::connect_client(std::string host, std::string server_port)
{
	sockfd = conman->create_connection(host, server_port);
	return sockfd > 0 && send_message("CONNECT", "");
}

bool client::send_message(std::string msg_name, std::string msg_payload)
{
	if (!connected()) {
		return false;
	}

	std::stringstream msg;

	msg << msg_name << " " << hostname << " " << msg_payload << std::endl;

	const std::string msg_str = msg.str();
	return conman->add_message(sockfd, msg_str);
}

bool client::set_nick(std::string new_nick)
{
	if (nick == "") {
		nick = new_nick;
	}
	if (!send_message("NICK", new_nick)) {
		return false;
	}
	nick = new_nick;

	return true;
}

bool client::join_channel(std::string chan_name)
{
	return send_message("JOIN", chan_name);
}

bool client::leave_channel(std::string chan_name)
{
	return send_message("LEAVE", chan_name);
}

bool client::connected()
{
	return sockfd > 0;
}

bool client::get_topic(std::string chan_name)
{
	return send_message("GETTOPIC", chan_name);
}

bool client::set_topic(std::string chan_name, std::string new_channel_topic)
{
	return send_message("SETTOPIC", chan_name + " " + new_channel_topic);
}

bool client::send_channel_message(std::string channel_name, std::string message)
{
	return send_message("MSG", nick + " " + channel_name + " " + message);
}

bool client::send_private_message(std::string recipient, std::string channel, std::string message)
{
	return send_message("PRIVMSG", nick + " " + channel + " " + recipient + " " + message);
}

void client::quit()
{
	quit_bit = true;
	set_socket_blocking(sockfd);
	std::ostringstream msg;
	msg << "QUIT" << " " << hostname << std::endl;
	send(sockfd, msg.str().c_str(), msg.str().size(), 0);
}

bool client::run()
{
	while (!quit_bit) {
		int count = conman->wait_events();

		if (count == -1) {
			perror("epoll_wait");
			return false;
		}

		struct epoll_event ev;

		while (conman->next_event(ev)) {
			if (ev.events & EPOLLRDHUP || ev.events & EPOLLERR || ev.events & EPOLLHUP) {
				conman->remove_socket(ev.data.fd);
				return false;
			} else if (ev.data.fd == STDIN_FILENO) {
				std::string cmd;
				std::getline(std::cin, cmd);
				if (process_command(cmd) == false) {
					std::cerr << "invalid command. see HELP" << std::endl;
				}
			} else if (ev.data.fd == sockfd) {

				if (ev.events & EPOLLIN) {
					if (conman->receive_messages(ev.data.fd)) {
						std::string msg;
						while (conman->fetch_message(ev.data.fd, msg)) {
							process_message(msg);
						}
					} else {
						conman->remove_socket(ev.data.fd);
						std::cerr << "connection closed" << std::endl;
					}

				} else if (ev.events & EPOLLOUT) {
					if (!conman->send_messages(ev.data.fd)) {
						std::cerr << "failed to send messages from queue" << std::endl;
					}
				}
			}
		}
	}
	return true;
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
					return set_nick(par1);
				} else {
					std::cerr << "usage: /NICK <new nick>" << std::endl;
				}
				break;
			case JOIN:
				if (cmd_stream >> par1) {
					return join_channel(par1);
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
					return get_topic(current_channel);
				}
				break;
			case SETTOPIC:
				if (current_channel == "") {
					std::cerr << "must be joined to channel";
					return false;
				} else if (getline(cmd_stream, par1, '\n')) {
					return set_topic(current_channel, par1.substr(1,par1.size()));
				} else {
					std::cerr << "usage: /SETTOPIC <topic>" << std::endl;
				}
				break;
			case MSG:
				if (std::getline(cmd_stream, par1, '\n')) {
					return send_channel_message(current_channel, par1.substr(1,par1.size()));
				} else {
					std::cerr << "usage: [/MSG] <message>" << std::endl;
				}
				break;
			case PRIVMSG:
				if (cmd_stream >> par1 && std::getline(cmd_stream, par2, '\n')) {
					return send_private_message(par1, current_channel, par2.substr(1, par2.size()));
				} else {
					std::cerr << "usage: /PRIVMSG <recipient> <message>" << std::endl;
				}
				break;
			case LIST:
				return send_list();
				break;
			case QUIT:
				quit();
				return true;
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
			return send_channel_message(current_channel, cmd + par1);
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

bool client::send_list()
{
	return send_message("LIST", "");
}

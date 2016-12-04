#include "server.hpp"
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

int main(int argc, char* argv[])
{
	std::string usage = "usage: ibrcd [-p <listen_port>] [-h <peer_host>] [-k <peer_port>]";
	std::string peer_port = DEFAULT_SERVER_PORT;
	std::string peer_host;
	std::string listen_port = DEFAULT_SERVER_PORT;

	int opt = 0;
	bool wants_connect = false;

	while ((opt = getopt(argc, argv, "p:h:k:")) != -1) {
		switch (opt) {
			case 'p':
				listen_port = optarg;
				break;
			case 'h':
				wants_connect = true;
				peer_host = optarg;
				break;
			case 'k':
				wants_connect = true;
				peer_port = optarg;
				break;
			default:
				std::cerr << usage << std::endl;
				exit(EXIT_FAILURE);
				break;
		}
	}

	try {
		server s = server(listen_port);

		if (wants_connect) {
			if (peer_host != "") {
				if (!s.connect_parent(peer_host, peer_port)) {
					std::cerr << "failed to connect" << std::endl;
					exit(EXIT_FAILURE);
				}
			} else {
				std::cerr << "usage: -k makes -h mandatory" << std::endl; 
			}
		}

		if (!s.run()) { // run the server
			exit(EXIT_FAILURE);
		}

	} catch (server_exception e) {
		std::cerr << e.what() << std::endl;
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

server::server(std::string port)
{
	conman = new connection_manager();
	accepting = conman->add_accepting(port);
	if (accepting == -1) {
		delete conman;
		throw server_exception();
	}
	parent = 0;
}

server::~server()
{
	delete conman;
}

bool server::connect_parent(std::string host, std::string port)
{
	if (parent != 0) {
		return false;
	} else {
		parent = conman->create_connection(host, port);
		return parent != 0;
	}
}

bool server::run()
{
	while (true) {
		// polling here
		int count_events = conman->wait_events();

		if (count_events == -1) {
			perror("epoll_wait");
			return false;
		}

		struct epoll_event ev;
		while (conman->next_event(ev)) {
			if (ev.events & EPOLLERR || ev.events & EPOLLHUP) {
				std::cerr << "epoll failed, quitting" << std::endl;
				return false;
			} else if (ev.data.fd == accepting) { // adds a new client or server
				int sock = conman->accept_client(ev.data.fd);
				if (sock == -1) {
					std::cerr << "failed to accept incoming connection"
					       	<< std::endl;
				}
			} else { // some other fd is ready
				if (ev.events & EPOLLIN) {
					if (conman->receive_messages(ev.data.fd)) {
						std::string msg;
						while (conman->fetch_message(ev.data.fd, msg)) {
							// reads all fully received messages
							// TODO remove
							std::cout << "receiving: " << msg;
							//
							process_message(msg, ev.data.fd);
						}
					} else {
						std::cerr << "failed to receive messages" << std::endl;
					}
				} else if (ev.events & EPOLLOUT) {
					if (!conman->send_messages(ev.data.fd)) {
						std::cerr << "failed to send messages from queue" << std::endl;
					}
				}
			}
		}
	}
	return false;
}

void server::process_message(std::string msg, int source)
{
	std::istringstream smsg (msg);
	msg_type type;
	std::string host;
	std::string port;

	if (smsg >> type) {
		switch (type) {
			case CONNECT:
				do_connect(smsg, source);
				break;
			case DISCONNECT:
				do_disconnect(smsg, source);
				break;
			case NICK:
				do_nick(smsg, source);
				break;
			case JOIN:
				do_join(smsg, source);
				break;
			case LEAVE:
				do_leave(smsg, source);
				break;
			case GETTOPIC:
				do_gettopic(smsg, source);
				break;
			case SETTOPIC:
				do_settopic(smsg, source);
				break;
			case MSG:
				do_msg(smsg, source);
				break;
			case PRIVMSG:
				do_privmsg(smsg, source);
				break;
			case STATUS:
				do_status(smsg, source);
				break;
			case TOPIC:
				do_topic(smsg, source);
				break;
			case NICKRES:
				do_nickres(smsg, source);
				break;
			default:
				// do_nothing
				break;
		}
	}
}

void server::send_status(const address *addr, status_code code)
{
	std::ostringstream reply;
	reply << "STATUS " << addr->host 
		<< " " << addr->port
		<< " " << static_cast<int>(code) << std::endl;
	conman->add_message(addr->route, reply.str());
}

void server::do_connect(std::istringstream &smsg, int source)
{
	std::string host, port;
	if (smsg >> host >> port) {
		address *naddr = new address(host, port, source);
		if (parent) {
			conman->add_message(parent, smsg.str());
			return;
		} else {
			send_status(naddr, connect_success);
		}
	}
}

void server::do_disconnect(std::istringstream &smsg, int source)
{
	std::ostringstream reply;
	std::string host, port;
	if (smsg >> host >> port) {
		address *src = address::get(host + " " + port);
		if (src == nullptr) {
			return;
		}
		if (src->route == source) {
			delete src;
			if (parent) {
				conman->add_message(parent, smsg.str());
			}
		} else {
			send_status(src, disconnect_error);
		}
	}
}

void server::do_nick(std::istringstream &smsg, int source)
{
	std::string host, port, nick;
	if (!(smsg >> host >> port >> nick)) {
		return;
	}
	address *src = address::get(host + " " + port);
	if (src == nullptr) {
		return;
	}
	if (src->route != source) {
		send_status(src, nick_not_authorized);
		return;
	}
	if (client_data::get(nick) == nullptr) { // nick unique
		if (parent) {
			conman->add_message(parent, smsg.str());
			return;
		}
		if (src->get_peer()->set_nick(nick)) {
			send_status(src, nick_unique);
			conman->add_message(src->route, "NICKRES " + host + " " + port + " " + nick);
			return;
		}
	} else {
		send_status(src, nick_not_unique);
		return;
	}
}

void server::do_join(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name;
	if (!(smsg >> host >> port >> chan_name)) {
		return;
	}
	address *src = address::get(host + " " + port);
	if (src == nullptr || src->route != source) {
		return;
	}
	if (src->get_peer()->get_nick() == "") {
		send_status(src, nick_not_set);
		return;
	}
	if (src->get_peer()->get_channel() != nullptr) {
		send_status(src, already_in_channel);
		return;
	}
	channel *chan = channel::get(chan_name);
	if (chan == nullptr) {
		// create channel
		if (parent) {
			chan->subscribe(src->route);
			conman->add_message(parent, smsg.str());
			return;
		}
		chan = new channel(chan_name, src->get_peer());
		src->get_peer()->set_channel(chan);
		send_status(src, join_new_success);
		send_channel(src, chan);
		return;
	} // channel exists
	chan->subscribe(src->route);
	send_status(src, join_known_success);
	send_channel(src, chan);
}

void server::send_channel(const address *src, const channel *chan)
{
	std::ostringstream reply;
	reply << "CHANNEL " << " " << src->host << " " << src->port << " " 
		<< chan->name << " " << chan->get_topic() << chan->op->get_nick();
	conman->add_message(src->route, reply.str());
}

void server::do_leave(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name;
	if (!(smsg >> host >> port >> chan_name)) {
		address *src = address::get(host + " " + port);
		if (src == nullptr) {
			return;
		}
		channel *chan = channel::get(chan_name);
		if (chan == nullptr) {
			send_status(src, no_such_channel);
			return;
		}
		auto subs = chan->get_subscribers();
		if (subs.find(src->route) == subs.end()) {
			send_status(src, not_in_channel);
			return;
		}
		chan->unsubscribe(src->route);
		send_status(src, leave_successful);
		if (subs.empty()) {
			if (parent) {
				conman->add_message(parent, smsg.str());
			}
			delete chan;
		}
	}
}

void server::do_gettopic(std::istringstream &smsg, int source)
{
}

void server::do_settopic(std::istringstream &smsg, int source)
{}

void server::do_msg(std::istringstream &smsge, int source)
{}

void server::do_privmsg(std::istringstream &smsg, int source)
{}

void server::do_status(std::istringstream &smsg, int source)
{}

void server::do_topic(std::istringstream &smsg, int source)
{}

void server::do_nickres(std::istringstream &smsg, int source)
{}

const char* server_exception::what() const throw()
{
	return "server: failed to create a server";
}
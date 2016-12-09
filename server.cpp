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
	std::string peer_port = DEFAULT_PORT;
	std::string peer_host;
	std::string listen_port = DEFAULT_PORT;

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
	parent = -1;
	root = true;
}

server::~server()
{
	delete conman;
}

bool server::connect_parent(std::string host, std::string port)
{
	parent = conman->create_connection(host, port);
	root = (parent == -1);
	return parent != -1;
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
			if (ev.events & EPOLLRDHUP) { // remote peer closed connection
				close_route(ev.data.fd);
				if (parent == ev.data.fd) { // become new root
					root = true;
				}
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
						close_route(ev.data.fd);
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
			case NICK:
				do_nick(smsg, source);
				break;
			case JOIN:
				do_join(smsg, source);
				break;
			case LEAVE:
				do_leave(smsg, source);
				break;
			case LIST:
				do_list(smsg, source);
			case LISTRES:
				do_listres(smsg, source);
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
			case CHANNEL:
				do_channel(smsg, source);
				break;
			case QUIT:
				do_quit(smsg, source);
				break;
			default:
				// do_nothing
				break;
		}
	}
}

void server::send_status(const peer *dest, status_code code)
{
	std::ostringstream reply;
	reply << "STATUS " << dest->get_nick() << " " << static_cast<int>(code) << std::endl;
	conman->add_message(dest->route, reply.str());
}

void server::do_nick(std::istringstream &smsg, int source)
{
	std::string nick, next_nick;
	if (!(smsg >> nick >> next_nick) || source == parent) {
		return;
	}
	peer *from = peer::get(nick);
	peer *to = peer::get(next_nick);

	if (to == nullptr) { // next nick free
		if (from == nullptr) { // peer is new
			if (nick.size() > 9) {
				return;
			}
			for (auto c : next_nick) {
				if (!std::isalnum(c)) {
					return;
				}
			}
			from = new peer(source, next_nick);
		} else { // try to modify existing
			if (from->route != source) {
				std::stringstream stat;
				stat << "STATUS " << next_nick << " " << static_cast<int>(nick_not_authorized) << std::endl;
				conman->add_message(source, stat.str());
				return; // abort
			} else { // update existing 
				from->set_nick(next_nick);
			}
		}

		if (root) {
			send_status(from, nick_unique);
		} else {
			conman->add_message(parent, smsg.str());
		}
	} else { // new nick not free
		std::stringstream stat;
		stat << "STATUS " << next_nick << " " << static_cast<int>(nick_not_unique) << std::endl;
		conman->add_message(source, stat.str());
		return; // abort
	}
}

void server::do_join(std::istringstream &smsg, int source)
{
	std::string nick, chan_name;
	if (!(smsg >> nick >> chan_name)) {
		return;
	}
	peer *src = peer::get(nick);
	if (src == nullptr || src->route != source) {
		return;
	}
	if (src->get_channel() != nullptr) {
		send_status(src, already_in_channel);
		return;
	}
	channel *chan = channel::get(chan_name);
	bool created = false;
	if (chan == nullptr) { // create channel
		if (chan_name.size() < 14) {
			chan = new channel(chan_name, src); // may be overwritten later
			created = true;
		} else {
			return;
		}
	}
	chan->subscribe(src->route);
	src->set_channel(chan);
	if (root) {
		send_channel(src, chan);
		if (created) {
			send_status(src, join_new_success);
		} else {
			send_status(src, join_known_success);
		}
	} else {
		conman->add_message(parent, smsg.str());
	}
}

void server::send_channel(const peer *dest, const channel *chan)
{
	std::ostringstream reply;
	if (chan->op != nullptr) {
		reply << "CHANNEL" << " " << dest->get_nick() << " " 
			<< chan->name << " " <<  chan->op->get_nick() << " " << chan->get_topic() << std::endl;
		conman->add_message(dest->route, reply.str());
	}
}

void server::do_leave(std::istringstream &smsg, int source)
{
	std::string nick, chan_name;
	if (!(smsg >> nick >> chan_name)) {
		return;
	}
	peer *src = peer::get(nick);
	if (src == nullptr || src->route != source) {
		return;
	}
	channel *chan = channel::get(chan_name);
	if (chan == nullptr) {
		send_status(src, no_such_channel);
	} else if (src->get_channel() != chan) {
		send_status(src, not_in_channel);
	} else {
		chan->unsubscribe(src->route);
		src->set_channel(nullptr); // leaves channel

		if (root) {
			send_status(src, leave_successful);
		} else {
			conman->add_message(parent, smsg.str());
		}

		if (chan->get_subscribers().empty()) {
			delete chan;
		}
	}
}

void server::do_gettopic(std::istringstream &smsg, int source)
{
	std::string nick, chan_name;
	if (!(smsg >> nick >> chan_name)) {
		return;
	}
	peer *src = peer::get(nick);
	if (src == nullptr || src->route != source) {
		return;
	}
	channel *chan = channel::get(chan_name);
	if (chan != nullptr) { // known channel, sending reply
		std::ostringstream reply;
		reply << "TOPIC " << nick << " " 
			<< chan_name << " " << chan->get_topic() << std::endl;
		conman->add_message(src->route, reply.str());
	} else if (!root) {
		conman->add_message(parent, smsg.str());
	} else {
		send_status(src, no_such_channel);
	}
}

void server::do_settopic(std::istringstream &smsg, int source)
{
	std::string msg = smsg.str();
	std::string nick, chan_name, topic;
	if (!(smsg >> nick >> chan_name && std::getline(smsg, topic, '\n'))) {
		return;
	}
	if (topic.size() > 0) { // leading space
		topic = topic.substr(1, topic.size());
	}
	peer *src = peer::get(nick);
	channel *chan = channel::get(chan_name);
	if (parent == source) {
 		if (chan != nullptr) {
			chan->set_topic(topic);
			send_to_channel(chan, msg, source);
		}
	} else if (src != nullptr) {
		if (chan == nullptr) {
			send_status(src, no_such_channel);
		} else if (chan->op == src && src->route == source) {
			chan->set_topic(topic);
			send_to_channel(chan, msg, source);
		} else {
			send_status(src, nick_not_authorized);
		}
	}
}

void server::do_msg(std::istringstream &smsg, int source)
{
	std::string chan_name, msg, sender;
	if (!(smsg >> sender >> chan_name)) {
		return;
	}
	channel *chan = channel::get(chan_name);
	peer *src = peer::get(sender);
	if (chan == nullptr && src != nullptr) {
		send_status(src, no_such_channel);
	} else if (src != nullptr && src->get_channel()->name != chan_name) {
		send_status(src, not_in_channel);
	} else if (root && src != nullptr) {
		send_status(src, msg_delivered);
	}
	send_to_channel(chan, smsg.str(), source);
}

void server::do_privmsg(std::istringstream &smsg, int source)
{
	std::string nick, chan_name, dest_nick;
	if (!(smsg >> nick >> chan_name >> dest_nick)) {
		return;
	}
	channel *chan = channel::get(chan_name);
	peer *src = peer::get(nick);
	if (chan == nullptr && src != nullptr) {
		send_status(src, no_such_channel);
	} else if (src != nullptr && src->get_channel()->name != chan_name) {
		send_status(src, not_in_channel);
	} else {
		peer *dest = peer::get(dest_nick);
		if (dest != nullptr) {
			conman->add_message(dest->route, smsg.str());
		} else {
			send_to_channel(chan, smsg.str(), source);
		}
		if (root) {
			send_status(src, privmsg_delivered);
		}
	}
}

void server::do_status(std::istringstream &smsg, int source)
{
	std::string dest_nick;
	status_code status;
	if (!(smsg >> dest_nick >> status)) {
		return;
	}
	peer *dest = peer::get(dest_nick);
	if (dest != nullptr) {
		conman->add_message(dest->route, smsg.str());
		/*if (dest != nullptr && status == nick_not_unique) {
			auto peers = socket_to_peers[dest->route];
			peers.erase(dest);
			delete dest;
		}*/
	}
}

void server::do_topic(std::istringstream &smsg, int source)
{
	std::string nick, chan_name, topic;
	if (!(smsg >> nick >> chan_name) || !(std::getline(smsg, topic, '\n'))) {
		return;
	}
	peer *dest = peer::get(nick);
	if (dest != nullptr) {
		conman->add_message(dest->route, smsg.str());
	}
}

void server::do_list(std::istringstream &smsg, int source)
{
	std::string nick;
	if (!(smsg >> nick)) {
	       return;
	}
	peer *src = peer::get(nick);
	if (src == nullptr || src->route != source) {
		return;
	}

	if (!root) {
		conman->add_message(parent, smsg.str());
	} else {
		std::ostringstream out;
		out << "LISTRES " << nick;
		for (auto name : channel::get_channel_list()) {
			out << " " << name;
		}
		out << std::endl;
		conman->add_message(src->route, out.str());
	}
}

void server::do_channel(std::istringstream &smsg, int source)
{
	std::string nick, chan_name, topic, op;
	if (!(smsg >> nick >> chan_name >> op && std::getline(smsg, topic, '\n')))  {
		return;
	}
	if (topic.size() > 0) { // leading space
		topic = topic.substr(1, topic.size());
	}
	peer *dest = peer::get(nick);
	if (source == parent && dest != nullptr) {
		channel *chan = channel::get(chan_name);
		peer *chanop = peer::get(op);
		if (chan != nullptr) {
			chan->op = chanop;
			chan->set_topic(topic);
		} else {
			chan = new channel(chan_name, chanop);
			chan->set_topic(topic);
		}
		chan->subscribe(dest->route);
		conman->add_message(dest->route, smsg.str());
	}
}

void server::do_listres(std::istringstream &smsg, int source)
{
	std::string nick;
	if (!(smsg >> nick)) {
		return;
	}
	peer *dest = peer::get(nick);
	if (dest != nullptr) {
		conman->add_message(dest->route, smsg.str());
	}
}

void server::send_to_channel(channel *chan, std::string msg, int source)
{
	auto subs = chan->get_subscribers();
	for (auto s : subs) {
		if (s != source) {
			conman->add_message(s, msg);
		}
	}
	if (!root && source != parent) {
		conman->add_message(parent, msg);
	}
}

const char* server_exception::what() const throw()
{
	return "server: failed to create a server";
}

void server::close_route(int sock)
{
	if (!root) {
		auto peers = peer::get_peers(sock);
		for (auto p : peers) {
			conman->add_message(parent, "QUIT " + p->get_nick());
		}
	}

	peer::close_route(sock);

	conman->remove_socket(sock);
}

void server::do_quit(std::istringstream &smsg, int source)
{
	std::string nick;
	if (!(smsg >> nick)) {
		return;
	}

	peer *src = peer::get(nick);

	if (src == nullptr || src->route != source) {
		return;
	}

	if (!root) {
		conman->add_message(parent, smsg.str());
	}

	delete src;
}

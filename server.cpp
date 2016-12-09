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
			case NICKRES:
				do_nickres(smsg, source);
				break;
			case CHANNEL:
				do_channel(smsg, source);
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
	reply << "STATUS " << dest->host 
		<< " " << dest->port
		<< " " << static_cast<int>(code) << std::endl;
	conman->add_message(dest->route, reply.str());
}

void server::do_connect(std::istringstream &smsg, int source)
{
	std::string host, port;
	if (!(smsg >> host >> port)) {
		return;
	}
	peer *src = new peer(host, port, source);
	socket_to_peers[source].insert(src);
	if (!root) {
		conman->add_message(parent, smsg.str());
	} else {
		send_status(src, connect_success);
	}
}

void server::do_disconnect(std::istringstream &smsg, int source)
{
	std::ostringstream reply;
	std::string host, port;
	if (!(smsg >> host >> port)) {
		return;
	}
	peer *src = peer::get(host, port);
	if (src == nullptr || src->route != source) { // bad message format
		return;
	}
	if (src->get_channel() != nullptr) { // unsubscribe from channel
		src->get_channel()->unsubscribe(source);
	}
	auto found = socket_to_peers.find(source);
	if (found != socket_to_peers.end()) {
		(*found).second.erase(src);
	}
	delete src;
	if (!root) {
		conman->add_message(parent, smsg.str());
	} else {
		send_status(src, disconnect_error);
	}
}

void server::do_nick(std::istringstream &smsg, int source)
{
	std::string host, port, nick;
	if (!(smsg >> host >> port >> nick)) {
		return;
	}
	peer *src = peer::get(host, port);
	peer *original_peer = peer::get(nick);
	if (src == nullptr || src->route != source) { // bad request
		return;
	} else if (root) {
		if (original_peer != nullptr && src != original_peer) { // nick not unique
			send_status(src, nick_not_unique);
		} else {
			send_status(src, nick_unique);
			src->set_nick(nick);
			conman->add_message(src->route, "NICKRES " + host + " " + port + " " + nick + '\n');
		}
	} else { // forward to parent
		conman->add_message(parent, smsg.str());
	}
}

void server::do_join(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name;
	if (!(smsg >> host >> port >> chan_name)) {
		return;
	}
	peer *src = peer::get(host, port);
	if (src == nullptr || src->route != source) {
		return;
	}
	if (src->get_nick() == "") {
		send_status(src, nick_not_set);
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
			chan = new channel(chan_name, src);
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
		reply << "CHANNEL" << " " << dest->host << " " << dest->port << " " 
			<< chan->name << " " <<  chan->op->get_nick() << " " << chan->get_topic() << std::endl;
		conman->add_message(dest->route, reply.str());
	}
}

void server::do_leave(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name;
	if (!(smsg >> host >> port >> chan_name)) {
		return;
	}
	peer *src = peer::get(host, port);
	if (src == nullptr || src->route != source) {
		return;
	}
	channel *chan = channel::get(chan_name);
	if (chan == nullptr) {
		send_status(src, no_such_channel);
		return;
	}
	if (src->get_channel()->name == chan_name) {
		send_status(src, not_in_channel);
		return;
	}
	src->set_channel(nullptr);
	chan->unsubscribe(src->route);
	send_status(src, leave_successful);
	if (chan->get_subscribers().empty()) {
		conman->add_message(parent, smsg.str());
		delete chan;
	}
}

void server::do_gettopic(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name;
	if (!(smsg >> host >> port >> chan_name)) {
		return;
	}
	peer *src = peer::get(host, port);
	if (src == nullptr || src->route != source) {
		return;
	}
	channel *chan = channel::get(chan_name);
	if (chan != nullptr) { // known channel, sending reply
		std::ostringstream reply;
		reply << "TOPIC " << host << " " << port << " " 
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
	std::string host, port, chan_name, topic;
	if (!(smsg >> host >> port >> chan_name && std::getline(smsg, topic, '\n'))) {
		return;
	}
	if (topic.size() > 0) { // leading space
		topic = topic.substr(1, topic.size());
	}
	peer *src = peer::get(host, port);
	channel *chan = channel::get(chan_name);

	if (parent == source) {
 		if (chan != nullptr) {
			chan->set_topic(topic);
			send_to_channel(chan, msg, source);
		}
	} else if (src != nullptr) {
		if (chan == nullptr) {
			send_status(src, no_such_channel);
		} else if (chan->op == src) {
			chan->set_topic(topic);
			send_to_channel(chan, msg, source);
		} else {
			send_status(src, nick_not_authorized);
		}
	}
}

void server::do_msg(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name, msg, sender;
	if (!(smsg >> host >> port >> chan_name >> sender) || !(std::getline(smsg, msg, '\n'))) {
		return;
	}
	if (msg.size() > 0) {
		msg = msg.substr(1, msg.size());
	}
	channel *chan = channel::get(chan_name);

	peer *src = peer::get(host, port);
	if (chan == nullptr && src != nullptr) {
		send_status(src, no_such_channel);
	} else if (src != nullptr && src->get_channel()->name != chan_name) {
		send_status(src, not_in_channel);
	} else {
		send_to_channel(chan, smsg.str(), source);
		if (root) {
			send_status(src, msg_delivered);
		}
	}
}

void server::do_privmsg(std::istringstream &smsg, int source)
{
	std::string host, port, nick, chan_name, msg, sender;
	if (!(smsg >> host >> port >> chan_name >> sender >> nick) || !(std::getline(smsg, msg, '\n'))) {
		return;
	}
	if (msg.size() > 0) {
		msg = msg.substr(1, msg.size());
	}
	channel *chan = channel::get(chan_name);
	peer *src = peer::get(host, port);
	if (chan == nullptr && src != nullptr) {
		send_status(src, no_such_channel);
	} else if (src != nullptr && src->get_channel()->name != chan_name) {
		send_status(src, not_in_channel);
	} else {
		peer *dest = peer::get(nick);
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
	std::string host, port;
	status_code status;
	if (!(smsg >> host >> port >> status)) {
		return;
	}
	peer *dest = peer::get(host, port);
	if (dest == nullptr) {
		if (source != parent) {
			conman->add_message(parent, smsg.str());
		}
	} else {
		conman->add_message(dest->route, smsg.str());
	}
}

void server::do_topic(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name, topic;
	status_code status;
	if (!(smsg >> host >> port >> chan_name) || !(std::getline(smsg, topic, '\n'))) {
		return;
	}
	peer *dest = peer::get(host, port);
	if (dest == nullptr) {
		if (source != parent) {
			conman->add_message(parent, smsg.str());
		} // else drop
	} else {
		conman->add_message(dest->route, smsg.str());
	}
}

void server::do_nickres(std::istringstream &smsg, int source)
{
	std::string host, port, nick;
	status_code status;
	if (!(smsg >> host >> port >> nick)) {
		return;
	}
	peer *dest = peer::get(host, port);
	if (source == parent && dest != nullptr) {
		dest->set_nick(nick);
		conman->add_message(dest->route, smsg.str());
	}
}

void server::do_list(std::istringstream &smsg, int source)
{
	std::string host, port;
	if (!(smsg >> host >> port)) {
	       return;
	}
	peer *src = peer::get(host, port);
	if (src == nullptr || src->route != source) {
		return;
	}
	if (!root) {
		conman->add_message(parent, smsg.str());
	} else {
		std::ostringstream out;
		out << "LISTRES " << host << " " << port;
		for (auto name : channel::get_channel_list()) {
			out << " " << name;
		}
		out << std::endl;
		conman->add_message(src->route, out.str());
	}
}

void server::do_channel(std::istringstream &smsg, int source)
{
	std::string host, port, chan_name, topic, op;
	if (!(smsg >> host >> port >> chan_name >> op && std::getline(smsg, topic, '\n')))  {
		return;
	}
	if (topic.size() > 0) { // leading space
		topic = topic.substr(1, topic.size());
	}
	peer *dest = peer::get(host, port);
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
	std::string host, port;
	if (!(smsg >> host >> port)) {
		return;
	}
	peer *dest = peer::get(host, port);
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

std::set<peer*> server::get_peers(int sock)
{
	return socket_to_peers[sock];
}

void server::close_route(int sock)
{
	auto peers_in_route = socket_to_peers[sock];
	for (peer *p : peers_in_route) {
		delete p;
	}
	socket_to_peers.erase(sock);
	conman->remove_socket(sock);
}

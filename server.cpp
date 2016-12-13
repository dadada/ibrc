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
			if (ev.events & EPOLLRDHUP || ev.events == EPOLLERR || ev.events == EPOLLHUP) { // remote peer closed connection
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
			case NICKRES:
				do_nickres(smsg, source);
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
			case DELCHANNEL:
				do_delchannel(smsg, source);
				break;
			default:
				// do_nothing
				break;
		}
	}
}

void server::do_delchannel(std::istringstream &smsg, int source)
{
	std::string chan_name;
	if (smsg >> chan_name) {
		channel *chan = channel::get(chan_name);
		if (chan != nullptr) {
			delete chan;
			if (root) {
				peer *op = peer::get_by_host(chan->op);
				send_status(op, leave_successful);
			}
		}
	}
}

void server::send_status(const peer *dest, status_code code)
{
	std::ostringstream reply;
	reply << "STATUS " << dest->host << " " << static_cast<int>(code) << std::endl;
	conman->add_message(dest->route, reply.str());
}

void server::do_connect(std::istringstream &smsg, int source)
{
	std::string host;

	if (smsg >> host && source != parent) {
		peer *npeer = new peer(source, host);
		if (!root) {
			conman->add_message(parent, smsg.str());
		} else {
			send_status(npeer, connect_success);
		}
	}
}

void server::do_disconnect(std::istringstream &smsg, int source)
{
	std::string host;

	if (smsg >> host && source != parent) {
		peer *deleted = peer::get_by_host(host);
		if (deleted != nullptr) {
			delete deleted;
			conman->add_message(parent, smsg.str());
		}
	}
}

void server::do_nick(std::istringstream &smsg, int source)
{
	std::string host, nick;

	if (smsg >> host >> nick) {
		peer *known = peer::get_by_host(host);

		if (known != nullptr) {
			peer *collision = peer::get(nick);

			if (collision != nullptr) {
				send_status(known, nick_not_unique);
			} else {
				if (root) {
					if (test_nick(nick)) {
						known->set_nick(nick);
						send_nick_res(known, nick);
						send_status(known, nick_unique);
					}
				} else {
					conman->add_message(parent, smsg.str());
				}
			}
		}
	}
}

void server::send_nick_res(peer *dest, std::string &nick)
{
	std::ostringstream msg;
	msg << "NICKRES" << " " << dest->host << " " << nick << std::endl;

	conman->add_message(dest->route, msg.str());
}

void server::do_nickres(std::istringstream &smsg, int source)
{
	std::string host, nick;
	if (smsg >> host >> nick && source == parent) {
		peer *known = peer::get_by_host(host);
		if (known != nullptr && test_nick(nick)) {
			known->set_nick(nick);
			conman->add_message(known->route, smsg.str());
		}
	}
}

bool server::test_nick(std::string nick)
{
	if (nick.size() > 9) {
		return false;
	}
	for (auto c : nick) {
		if (!std::isalnum(c)) {
			return false;
		}
	}
	return true;
}

void server::do_join(std::istringstream &smsg, int source)
{
	std::string host, chan;

	if (smsg >> host >> chan) {
		peer *known_peer = peer::get_by_host(host);
		channel *known_channel = channel::get(chan);

		if (known_peer != nullptr) {
			if (known_peer->get_nick() == "") {
				send_status(known_peer, nick_not_set);
			} else if (channel::is_in_channel(known_peer)) {
				send_status(known_peer, already_in_channel);
			}
			else if (known_channel != nullptr) { // knows the channel
				known_channel->join(known_peer);
				send_channel(known_peer, known_channel);
			} else {
				if (root) {
					known_channel = new channel(chan, known_peer);
					send_channel(known_peer, known_channel);
				} else {
					conman->add_message(parent, smsg.str());
				}
			}
		}
	}
}

void server::send_channel(const peer *dest, const channel *chan)
{
	std::ostringstream reply;
	if (chan->op != "") {
		reply << "CHANNEL" << " " << dest->host << " " 
			<< chan->name << " " <<  chan->op << " " 
			<< chan->get_topic() << std::endl;
		conman->add_message(dest->route, reply.str());
	}
}

void server::do_leave(std::istringstream &smsg, int source)
{
	std::string host, chan_name;
	if (smsg >> host >> chan_name) {
		peer *src = peer::get_by_host(host);
		if (src != nullptr && src->route == source) {
			channel *chan = channel::get(chan_name);
			if (chan == nullptr) {
				send_status(src, no_such_channel);
			} else if (!chan->in_channel(src)) {
				send_status(src, not_in_channel);
			} else {
				chan->leave(src);

				if (chan->op == src->host) {
					send_delete_channel(chan, source);
					delete chan;
				} else if (!root) {
					conman->add_message(parent, smsg.str());
				}

				if (root) {
					send_status(src, leave_successful);
				}
			}
		}
	}
}


void server::send_delete_channel(channel *chan, int source)
{
	std::ostringstream del_msg;
	del_msg << "DELCHANNEL" << " " << chan->name << std::endl;
	send_to_channel(chan, del_msg.str(), source);
}

void server::do_gettopic(std::istringstream &smsg, int source)
{
	std::string host, chan_name;
	if (smsg >> host >> chan_name) {
		peer *src = peer::get_by_host(host);
		if (src != nullptr && src->route == source) {
			channel *chan = channel::get(chan_name);
			if (chan != nullptr && chan->in_channel(src)) { // known channel, sending reply
				send_topic(chan, src);
			} else {
				send_status(src, not_in_channel);
			}
		}
	}
}

void server::send_topic(channel *chan, peer *dest)
{
	std::ostringstream reply;
	reply << "TOPIC " 
		<< dest->host << " " 
		<< chan->name << " " 
		<< chan->get_topic() << std::endl;
	conman->add_message(dest->route, reply.str());
}

void server::do_settopic(std::istringstream &smsg, int source)
{
	std::string host, chan_name, topic;
	if (smsg >> host >> chan_name && std::getline(smsg, topic, '\n')) {
		if (topic.size() > 0) { // leading space
			topic = topic.substr(1, topic.size());
		}
		peer *src = peer::get_by_host(host);
		channel *chan = channel::get(chan_name);
		if (parent == source) {
			if (chan != nullptr) {
				chan->set_topic(topic);
				send_to_channel(chan, smsg.str(), source);
			}
		} else if (src != nullptr && src->route == source) {
			if (chan == nullptr) {
				send_status(src, no_such_channel);
			} else if (chan->op == src->host) {
				chan->set_topic(topic);
				send_to_channel(chan, smsg.str(), source);
			} else {
				send_status(src, nick_not_authorized);
			}
		}
	}
}

void server::do_msg(std::istringstream &smsg, int source)
{
	std::string chan_name, msg, sender, nick;
	if (smsg >> sender >> nick >> chan_name) {
		channel *chan = channel::get(chan_name);
		peer *src = peer::get_by_host(sender);

		if (chan == nullptr) {
			if (src != nullptr) {
				send_status(src, no_such_channel);
			}
		} else { // knows the channel
			if (src != nullptr) {
				if (src->route == source) { // validate source
					send_to_channel(chan, smsg.str(), source);
					if (root) {
						send_status(src, msg_delivered);
					}
				}
			} else if (source == parent) {
				send_to_channel(chan, smsg.str(), source);
			}
		}
	}
}

void server::do_privmsg(std::istringstream &smsg, int source)
{
	std::string host, sender, sender_nick, chan_name, dest_nick;
	if (smsg >> host >> sender >> sender_nick >> chan_name >> dest_nick) {
		channel *chan = channel::get(chan_name);
		peer *src = peer::get_by_host(sender);
		peer *dest = peer::get(dest_nick);

		if (source == parent) {
			if (chan != nullptr && dest != nullptr) {
				conman->add_message(dest->route, smsg.str());
			}
		} else {
			if (chan != nullptr && src != nullptr) {
				if (chan->in_channel(src)) {
					send_to_channel(chan, smsg.str(), source);
					if (root) {
						send_status(src, privmsg_delivered);
					}
				} else {
					send_status(src, not_in_channel);
				}
			} else if (src != nullptr) {
				send_status(src, no_such_channel);
			}
		}
	}
}

void server::do_status(std::istringstream &smsg, int source)
{
	std::string host;
	status_code status;
	if (smsg >> host >> status) {
		peer *dest = peer::get_by_host(host);
		if (dest != nullptr) {
			conman->add_message(dest->route, smsg.str());
		}
	}
}

void server::do_topic(std::istringstream &smsg, int source)
{
	std::string host, chan_name, topic;
	if (smsg >> host >> chan_name && std::getline(smsg, topic, '\n')) {
		peer *dest = peer::get_by_host(host);
		if (dest != nullptr) {
			conman->add_message(dest->route, smsg.str());
		}
	}
}

void server::do_list(std::istringstream &smsg, int source)
{
	std::string host;
	if (smsg >> host) {
		peer *src = peer::get_by_host(host);
		if (src != nullptr && src->route == source) {
			if (!root) {
				conman->add_message(parent, smsg.str());
			} else {
				send_channel_list(src);
			}
		}
	}
}

void server::send_channel_list(peer *dest)
{
	std::ostringstream out;
	out << "LISTRES " << dest->host;
	for (auto c : channel::channel_list()) {
		out << " " << c->name;
	}
	out << std::endl;
	conman->add_message(dest->route, out.str());
}

void server::do_channel(std::istringstream &smsg, int source)
{
	std::string host, chan_name, topic, op;
	if (smsg >> host >> chan_name >> op && std::getline(smsg, topic, '\n'))  {
		if (topic.size() > 0) { // leading space
			topic = topic.substr(1, topic.size());
		}
		peer *dest = peer::get_by_host(host);
		if (source == parent && dest != nullptr) {
			channel *chan = channel::get(chan_name);
			if (chan != nullptr) {
				chan->set_topic(topic);
			} else {
				chan = new channel(topic, chan_name, op);
			}
			chan->join(dest);
			conman->add_message(dest->route, smsg.str());
		}
	}
}

void server::do_listres(std::istringstream &smsg, int source)
{
	std::string host;
	if (smsg >> host) {
		peer *dest = peer::get_by_host(host);
		if (dest != nullptr) {
			conman->add_message(dest->route, smsg.str());
		}
	}
}

void server::send_to_channel(channel *chan, std::string msg, int source)
{
	auto subs = chan->get_routes();
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
	auto peers = peer::get_peers(sock);

	for (auto p : peers) {
		if (!root) {
			conman->add_message(parent, "QUIT " + p->host);
		}
		delete p;
	}

	conman->remove_socket(sock);
}

void server::do_quit(std::istringstream &smsg, int source)
{
	std::string host;

	if (smsg >> host) {
		peer *src = peer::get_by_host(host);

		if (src != nullptr && src->route == source) {

			if (!root) {
				conman->add_message(parent, smsg.str());
			}

			delete src;
		}
	}
}


#include "data.hpp"
#include <unordered_map>

std::unordered_map<std::string, peer*> peer::nick_to_peer;

std::unordered_map<std::string, channel*> channel::name_to_channel;


std::unordered_map<int, std::set<peer*>> peer::socket_to_peers;

std::set<peer*> peer::get_peers(int sock)
{
	return socket_to_peers[sock];
}

peer::peer(const int r, std::string name)
	: nick(name), route(r)
{
	nick_to_peer[name] = this;
	auto sockpeers = socket_to_peers[r];
	sockpeers.insert(this);
	chan = nullptr;
}

peer::~peer()
{
	auto found = nick_to_peer.find(nick);
	if (found != nick_to_peer.end()) {
		nick_to_peer.erase(found);
	}

	auto peers_in_route = socket_to_peers[route];

	peers_in_route.erase(this);

	if (peers_in_route.empty()) {
		close_route(route);
	}
}

void peer::close_route(int sock)
{
	auto peers_in_route = socket_to_peers[sock];

	for (peer *p : peers_in_route) {
		if (p->chan != nullptr) {
			p->chan->unsubscribe(sock);
		}
		delete p;
	}

	socket_to_peers.erase(sock);
}

peer* peer::get(std::string name)
{
	auto found = nick_to_peer.find(name);
	if (found != nick_to_peer.end()) {
		return (*found).second;
	} else {
		return nullptr;
	}
}

channel::channel(const std::string channel_name, const peer *channel_op)
	: name(channel_name), op(channel_op)
{
	name_to_channel.insert({channel_name, this});
	topic = "";
}

std::string channel::get_topic() const
{
	return topic;
}

void channel::set_topic(std::string topic_text)
{
	topic = topic_text;
}

std::set<int> channel::get_subscribers() const
{
	return subscribers;
}

void channel::subscribe(int peersock)
{
	subscribers.insert(peersock);
}

const char* receive_error::what() const throw()
{
	return "receive: failed to receive message";
}

std::ostream &operator<<(std::ostream &out, const msg_type &cmd) {
	out << command_names[cmd];
	return out;
}

std::istream &operator>>(std::istream &in, msg_type &cmd) {
	static std::unordered_map<std::string, msg_type> names = {
			{"CONNECT", CONNECT},
			{"DISCONNECT", DISCONNECT},
                        {"NICK", NICK},
                        {"JOIN", JOIN},
                        {"LEAVE", LEAVE},
                        {"LIST", LIST},
                        {"LISTRES", LISTRES},
                        {"GETTOPIC", GETTOPIC},
                        {"SETTOPIC", SETTOPIC},
                        {"MSG", MSG},
                        {"PRIVMSG", PRIVMSG},
                        {"QUIT", QUIT},
                        {"HELP", HELP},
                        {"STATUS", STATUS},
                        {"TOPIC", TOPIC},
                        {"NICKRES", NICKRES},
                        {"CHANNEL", CHANNEL},
			{"connect", CONNECT},
			{"disconnect", DISCONNECT},
                        {"nick", NICK},
                        {"join", JOIN},
                        {"leave", LEAVE},
                        {"list", LIST},
                        {"listres", LISTRES},
                        {"gettopic", GETTOPIC},
                        {"settopic", SETTOPIC},
                        {"msg", MSG},
                        {"privmsg", PRIVMSG},
                        {"quit", QUIT},
                        {"help", HELP},
                        {"status", STATUS},
                        {"topic", TOPIC},
                        {"nickres", NICKRES},
                        {"channel", CHANNEL}
	};
	std::string name;
	in >> name;
	cmd = names[name];
	return in;
}

std::ostream &operator<<(std::ostream &out, const status_code &code)
{
	switch (code) {
		case connect_success:
			out << "connected successfully";
			break;
		case connect_error:
			out << "connection failed";
			break;
		case disconnect_success:
			out << "disconnected successfully";
			break;
		case disconnect_error:
			out << "failed to disconnect";
			break;
		case nick_unique:
			out << "nick unique, success";
			break;
		case nick_not_authorized:
			out << "not authorized to set nick";
			break;
		case nick_not_unique:
			out << "nick not unique, pick another nick";
			break;
		case join_known_success:
			out << "channel exists, joined channel";
			break;
		case join_new_success:
			out << "new channel created, joined channel, channel op";
			break;
		case not_in_channel:
			out << "must be joined to channel for this command";
			break;
		case no_such_channel:
			out << "no such channel";
			break;
		case already_in_channel:
			out << "client can only be joined to one channel at a time";
			break;
		case leave_successful:
			out << "left channel successfully";
			break;
		case client_not_channel_op:
			out << "must be channel op to perform this action";
			break;
		case msg_delivered:
			out << "message delivered successfully";
			break;
		case no_such_client:
			out << "client not found in network";
			break;
		case no_such_client_in_channel:
			out << "client is joined to different channel";
			break;
		case privmsg_delivered:
			out << "successfully delivered private message";
			break;
		case nick_not_set:
			out << "nick not set. try NICK <name>";
			break;
		default:
			out << "an unknown error occurred";
			break;
	}
	return out;
}

std::istream &operator>>(std::istream &in, status_code &status)
{
	int code;
	in >> code;
	if (code < 1000 && code > 99) {
		status = static_cast<status_code> (code);
	}
	return in;
}


std::string peer::get_nick() const
{
	return nick;
}

bool peer::set_nick(std::string nick_name)
{
	if (nick_name.size() > 9) {
		return false;
	}
	for (char c : nick_name) {
		if (!std::isalnum(c)) {
			return false;
		}
	}
	nick = nick_name;
	// TODO delete old nick here? would allow reusing nicks
	nick_to_peer[nick_name] = this;
	return true;
}

channel::~channel()
{
	auto found = name_to_channel.find(name);
	if (found != name_to_channel.end()) {
		name_to_channel.erase(found);
	}
}

bool channel::check_subscribed(int sockfd)
{
	return (subscribers.find(sockfd) != subscribers.end());
}

void channel::unsubscribe(int sockfd)
{
	subscribers.erase(sockfd);
}

std::vector<std::string> channel::get_channel_list()
{
	std::vector<std::string> chan_names;

	for (auto c : name_to_channel) {
		chan_names.push_back(c.first);
	}

	return chan_names;
}

void peer::set_channel(channel *c)
{
	chan = c;
}

channel* peer::get_channel() const
{
	return chan;
}

channel * channel::get(std::string chan_name)
{
	auto found = name_to_channel.find(chan_name);
	if (found != name_to_channel.end()) {
		return (*found).second;
	} else {
		return nullptr;
	}
}

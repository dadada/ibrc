#include "data.hpp"
#include <unordered_map>

std::unordered_map<std::string, address*> address::host_port_to_addr;

std::unordered_map<std::string, channel*> channel::name_to_channel;

std::unordered_map<std::string, client_data*> client_data::nick_to_client;

address::address(const std::string hostname, const std::string portnum, int r)
	: host(hostname), port(portnum), route(r)
{
	host_port_to_addr.insert({hostname + " " + port, this});
	peer = new client_data(this, "");
}

address::~address()
{
	auto found = host_port_to_addr.find(host + " " + port);
	if (found != host_port_to_addr.end()) {
		host_port_to_addr.erase(found);
	}
	delete peer;
}

client_data* address::get_peer() const
{
	return peer;
}

address* address::get(std::string name)
{
	auto found = host_port_to_addr.find(name);
	if (found != host_port_to_addr.end()) {
		return (*found).second;
	} else {
		return nullptr;
	}
}

channel::channel(const std::string channel_name, std::string channel_op)
	: name(channel_name), op(channel_op) 
{
	name_to_channel.insert({channel_name, this});
	//subscribe(op->addr->route);
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
                        {"CHANNEL", CHANNEL}
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

client_data::client_data(address *a, std::string nick_name) 
	: nick(nick_name), addr(a)
{
	nick_to_client.insert({nick_name, this});
}

std::string client_data::get_nick() const
{
	return nick;
}

bool client_data::set_nick(std::string nick_name)
{
	for (char c : nick_name) {
		if (!std::isalnum(c)) {
			return false;
		}
	}
	nick_to_client.erase(nick);
	nick_to_client.insert({nick_name, this});
	nick = nick_name;
	return true;
}

client_data* get_client_data(std::string host, std::string port) {
	address *source = address::get(host + " " + port);
	if (source != nullptr) {
		return source->get_peer();
	}
	return nullptr;
}

channel::~channel()
{
	auto found = name_to_channel.find(name);
	if (found != name_to_channel.end()) {
		name_to_channel.erase(found);
	}
}

client_data::~client_data()
{
	auto found = nick_to_client.find(nick);
	if (found != nick_to_client.end()) {
		nick_to_client.erase(found);
	}
}

client_data* client_data::get(std::string name)
{
	auto data = nick_to_client.find(name);
	if (data == nick_to_client.end()) {
		return nullptr;
	}
	return (*data).second;
}

channel* channel::get(std::string name)
{
	auto data = name_to_channel.find(name);
	if (data == name_to_channel.end()) {
		return nullptr;
	}
	return (*data).second;
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

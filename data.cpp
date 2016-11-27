#include "data.hpp"

address::address(std::string hostname, std::string portnum)
	: host(hostname), port(portnum)
{}

channel::channel(std::string channel_name, address channel_op)
	: name(channel_name), topic(""), op(channel_op) 
{}

const std::string channel::get_topic() const
{
	return topic;
}

void channel::set_topic(std::string topic_text)
{
	topic = topic_text;
}

const std::string channel::get_name() const
{
	return name;
}

const address channel::get_op()
{
	return op;
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
                        {"GETTOPIC", GETTOPIC},
                        {"SETTOPIC", SETTOPIC},
                        {"MSG", MSG},
                        {"PRIVMSG", PRIVMSG},
                        {"QUIT", QUIT},
                        {"HELP", HELP},
                        {"STATUS", STATUS},
                        {"TOPIC", TOPIC},
                        {"NICKRES", NICKRES}
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


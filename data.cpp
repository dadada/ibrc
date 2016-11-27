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
                        {"HELP", HELP}
	};
	std::string name;
	in >> name;
	cmd = names[name];
	return in;
}

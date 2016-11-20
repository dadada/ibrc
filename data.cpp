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
